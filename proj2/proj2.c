/**					
 * @file proj2.c					
 * @name Operacne systemy projekt 2					
 * @brief Tema: praca s procesmi a semaformi - riesenie synchronozacneho scenaru: "Faneuil Hall Problem"					
 * @author Lukas Tkac(xtkacl00@stud.fit.vutbr.cz)					
 *  					
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/** semafory */
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
/** shared memory */
#include <sys/ipc.h>
#include <sys/shm.h>

/**					
 * DEFINICIE KLUCOV PRE SEMAFORY 					
 */
#define SEM_PRINT "xtkacl00_sem_print"
#define SEM_JUDGE_ENTER "xtkacl00_sem_judge_enter"
#define SEM_MUTEX "xtkacl00_sem_mutex"
#define SEM_ALLSIGIN "xtkacl00_sem_allsigin"
#define SEM_CONF "xtkacl00_sem_conf"
#define SEM_GUARD "xtkacl00_sem_guard"

/**					
 * INICIALIZACIA SEMAFOROV pre procesy					
 */
sem_t *semaphore_print;
sem_t *semaphore_noJudge;
sem_t *semaphore_mutex;
sem_t *semaphore_allSignedIn;
sem_t *semaphore_confirmed;

/**					
 * INICIALIZACIA SEMAFORV pre zdielanu pamet 					
 */
sem_t *semaphore_guard;

/**					
 * DEFINOVANIE STRUKTURY PRE ZDIELANE ZDROJE A VSTUPNE PARAMETRE 					
 */
typedef struct shm_resources {
	int shm_interactions_counter;
	int shm_immigrants_counter;
	int shm_immigrants_enters;
	int shm_immigrants_checks;
	int shm_immigrants_balance;

	int shm_judge_entered;
} shm_rsc;

typedef struct arguments {
	int data[5];
	int PI;
	int IG;
	int JG;
	int IT;
	int JT;
} arguments_t;

FILE *output_file;
int shm_id;
shm_rsc *shared_resources;

/** 					
 * Macro pre vypis referencia: https://gcc.gnu.org/onlinedocs/cpp/Variadic-Macros.html					
 * pozn. vypis cez volanie funkcie by mohlo zaniest do behu programu asynchronne chovanie					
 */

#define PRINT_OUT(text, ...) do {\
		sem_wait(semaphore_print);\
		fprintf(output_file, text, __VA_ARGS__);\
		fflush(output_file);\
		sem_post(semaphore_print);\
	} while (0)

/**					
 * @brief Navod na pouzitie pre uzivatela pri nezadany spravnych vstupnych parametrov					
 */
void helper();
/**					
 * @brief Generator pristahovalcov					
 * @param num_of_immigrants pocet generovanych procesov v kategorii "imigrants".					
 * @param ttg_next_imm "time to generate" ku vygenerovaniu dalsieho pristahovalca. Ak ttg_next_imm == 0, tak sa vsetky procesy v kategorii "imigrants" generuju ihned.					
 * @param tt_certificated "time to" casovac pre simulovanie "nahodnej" doby vyzdvihnutia certifikatu pristahovalcom 					
 */
void cleanup();

void generate_immigrants(int num_of_immigrants, int ttg_next_imm, int tt_certificated);

/**					
 * @brief Proces pre pristahovalca					
 * @param tt_certificated "time to" casovac pre simulovanie "nahodnej" doby vyzdvihnutia certifikatu pristahovalcom 					
 */
void proc_immigrant(int tt_certificated, int identifi);

/**					
 * @brief Proces pre sudcu 					
 * @param tts_judge "time to spawn" ku simulovaniu pripravy vstupenia sudcu do budovy					
 */
void proc_judge(int tts_judge, int tt_judgement, int cnt_procend);

int main(int argc, char **argv) {
	//cleanup();
	/**					
	 * @brief deklaracia vstupnych argumentov(vsetky parametry jsou cela cisla)					
	 * @param PI je pocet procesu vygenerovaných v kategorii pristehovalcu; bude postupne vytvoreno PI immigrants. PI >= 1					
	 * @param IG je maximální hodnota doby (v milisekundách), po které je generován nový proces immigrant. IG >= 0 && IG <= 2000. 					
	 * @param JG je maximální hodnota doby (v milisekundách), po které soudce opet vstoupí do budovy. JG >= 0 && JG <= 2000.					
	 * @param IT je maximální hodnota doby (v milisekundách), která simuluje trvání vyzvedávání certifikátu pristehovalcem. IT >= 0 && IT <= 2000.					
	 * @param JT je maximální hodnota doby (v milisekundách), která simuluje trvání vydávání rozhodnutí soudcem. JT >= 0 && JT <= 2000.					
	 */

	if (argc != 6) {
		fprintf(stderr, "Nespravny pocet argumentov(nezadal si ich 5)\n");
		helper();
		return EXIT_FAILURE;
	}
	/** prvotne parsovanie argumentov (nekontrolujem intervaly iba pocet argumentov a datovy typ vstupnych argumentov) */
	arguments_t parsed_args;
	for (int i = 1; i < argc; i++) {
		/** konvertovanie na stringu na long integer, priprava na naslednu kontrolu, ci vazne ide o datovy typ int */
		char *dummy = '\0';
		parsed_args.data[i - 1] = strtol(argv[i], &dummy, 10);
		if (*dummy != '\0') {
			fprintf(stderr, "Vstupny argument musi byt cele cislo\n");
			helper();
			return EXIT_FAILURE;
		}
	}

	parsed_args.PI = parsed_args.data[0];
	/** PI */
	if (parsed_args.PI < 1) {
		fprintf(stderr, "Nesplna interval(argument PI)\n");
		helper();
		return EXIT_FAILURE;
	}
	parsed_args.IG = parsed_args.data[1];
	/** IG */
	if (parsed_args.IG < 0 || parsed_args.IG > 2000) {
		fprintf(stderr, "Nesplna interval(argument IG)\n");
		helper();
		return EXIT_FAILURE;
	}
	parsed_args.JG = parsed_args.data[2];
	/** JG */
	if (parsed_args.JG < 0 || parsed_args.JG > 2000) {
		fprintf(stderr, "Nesplna interval(argument JG)\n");
		helper();
		return EXIT_FAILURE;
	}
	parsed_args.IT = parsed_args.data[3];
	/** IT */
	if (parsed_args.IT < 0 || parsed_args.IT > 2000) {
		fprintf(stderr, "Nesplna interval(argument IT)\n");
		helper();
		return EXIT_FAILURE;
	}
	parsed_args.JT = parsed_args.data[4];
	/** JT */
	if (parsed_args.JT < 0 || parsed_args.JT > 2000) {
		fprintf(stderr, "Nesplna interval(argument JT)\n");
		helper();
		return EXIT_FAILURE;
	}

	output_file = fopen("proj2.out", "w+");
	if (output_file == NULL) {
		fprintf(stderr, "Chyba pri otvarani suboru\n");
		return EXIT_FAILURE;
	}
	/**					
 * INICIALIZACIA 					
 */
	pid_t judge_pid;
	pid_t immmigrant_pid;
	pid_t temporary_pid;

	/** alokovanie zdielanych zdrojov pamete pre procesy */
	shm_id = shmget(IPC_PRIVATE, sizeof(shm_rsc), IPC_CREAT | 0666);

	if (shm_id < 0) {
		fprintf(stderr, "Nepodarilo sa alokovat zdielane zdroje pamete pre procesy\n");
		perror("SHMGET");
		if (output_file != NULL)
			fclose(output_file);
		return EXIT_FAILURE;
	}
	/** pripojenie "System V" pametoveho segmentu identifikovaneho klucom ku adresnemu priestoru volajucedho procesu (podrobnejsie "man shmmat") */
	shared_resources = shmat(shm_id, NULL, 0);
	shared_resources->shm_interactions_counter = 1;
	//shared_resources->shm_immigrants_counter = 0;
	shared_resources->shm_immigrants_enters = 0;
	shared_resources->shm_immigrants_checks = 0;
	shared_resources->shm_immigrants_balance = 0;
	shared_resources->shm_judge_entered = 0;

	/**					
	 * inicializacia SEMAFOROV					
	 */
	semaphore_print = sem_open(SEM_PRINT, O_CREAT, S_IWUSR | S_IRUSR, 1);
	semaphore_noJudge = sem_open(SEM_JUDGE_ENTER, O_CREAT, S_IWUSR | S_IRUSR, 1);
	semaphore_mutex = sem_open(SEM_MUTEX, O_CREAT, S_IWUSR | S_IRUSR, 1);
	semaphore_allSignedIn = sem_open(SEM_ALLSIGIN, O_CREAT, S_IWUSR | S_IRUSR, 0);
	semaphore_confirmed = sem_open(SEM_CONF, O_CREAT, S_IWUSR | S_IRUSR, 0);
	semaphore_guard = sem_open(SEM_GUARD, O_CREAT, S_IWUSR | S_IRUSR, 1);

	if (semaphore_print == SEM_FAILED || semaphore_noJudge == SEM_FAILED || semaphore_mutex == SEM_FAILED ||
		semaphore_allSignedIn == SEM_FAILED || semaphore_confirmed == SEM_FAILED || semaphore_guard == SEM_FAILED) {

		sem_close(semaphore_print);
		sem_close(semaphore_noJudge);
		sem_close(semaphore_mutex);
		sem_close(semaphore_allSignedIn);
		sem_close(semaphore_confirmed);
		sem_close(semaphore_guard);
		return EXIT_FAILURE;
	}

	/** ***********************************************************************************************************/

	if ((temporary_pid = fork()) < 0) {
		perror("fork");
		shmctl(shm_id, IPC_RMID, NULL);
		if (output_file != NULL) {
			fclose(output_file);
		}
		exit(1);
	}
	/** IMM */
	if (temporary_pid == 0) {
		proc_judge(parsed_args.JG, parsed_args.JT, parsed_args.PI);
		exit(0);
	}
	else {
		judge_pid = temporary_pid;

		if ((temporary_pid = fork()) < 0) {
			perror("fork");
			exit(1);
		}
		if (temporary_pid == 0) { // child process
			generate_immigrants(parsed_args.PI, parsed_args.IG, parsed_args.IT);
			exit(0);
		}
		else {
			immmigrant_pid = temporary_pid;
		}
	}

	waitpid(immmigrant_pid, NULL, 0);
	waitpid(judge_pid, NULL, 0);
	cleanup();

	return 0;
}

void helper() {
	fprintf(stderr, "SPUSTENI: ./proj2 PI IG JG IT JT\n");
	fprintf(stderr, "PI je pocet procesu vygenerovaných v kategorii pristehovalcu; bude postupne vytvoreno PI immigrants. PI >= 1\n");
	fprintf(stderr, "IG je maximální hodnota doby (v milisekundách), po které je generován nový proces immigrant. IG >= 0 && IG <= 2000.\n");
	fprintf(stderr, "JG je maximální hodnota doby (v milisekundách), po které soudce opet vstoupí do budovy. JG >= 0 && JG <= 2000.\n");
	fprintf(stderr, "IT je maximální hodnota doby (v milisekundách), která simuluje trvání vyzvedávání certifikátu pristehovalcem. IT >= 0 && IT <= 2000.\n");
	fprintf(stderr, "JT je maximální hodnota doby (v milisekundách), která simuluje trvání vydávání rozhodnutí soudcem. JT >= 0 && JT <= 2000.\n");
	fprintf(stderr, "VSECHNY PARAMETRY JSOU CELA CISLA\n");
	exit(1);
}

void generate_immigrants(int num_of_immigrants, int ttg_next_imm, int tt_certificated) {
	pid_t imms_pid[num_of_immigrants];
	// generovanie imigrantov
	for (int i = 0; i < num_of_immigrants; i++) {
		pid_t immigrant_pid;

		if ((immigrant_pid = fork()) < 0) {
			perror("fork");
			shmctl(shm_id, IPC_RMID, NULL);
			if (output_file != NULL) {
				fclose(output_file);
			}
			exit(1);
		}
		if (immigrant_pid == 0) {
			proc_immigrant(tt_certificated, i);
			exit(0);
		}
		else {
			imms_pid[i] = immigrant_pid;
			int wait_time;
			if (ttg_next_imm != 0) {
				wait_time = rand() % ttg_next_imm; //time to generate next imigrant
			}
			wait_time = ttg_next_imm;
			/**					
			 *  konvertovanie mikro na milisekundy(ako je definovana maximalna doba cakania na dalsie generovanie podla IG)					
			 *	Cakanie na vytvorenie procesu imigranta					
			 *	usleep() je thread safe podla manualu (preto som nepouzil sleep() - o nom manualy nemaju uvedene toto info )					
			 */
			usleep(wait_time * 1000);
		}
	}

	// cakanie dokym vsetci imigranti budu vygenerovany
	for (int i = 0; i < num_of_immigrants; i++) {
		waitpid(imms_pid[i], NULL, 0);
	}
}
void proc_immigrant(int tt_certificated, int identifi) {
	int wait_time;
	if (tt_certificated != 0) {
		wait_time = rand() % tt_certificated; //time to generate next imigrant
	}
	wait_time = tt_certificated;

	sem_wait(semaphore_noJudge);


	PRINT_OUT("%d: IMM %d: starts\n", (shared_resources->shm_interactions_counter)++, ++(identifi));

	sem_wait(semaphore_guard);
	(shared_resources->shm_immigrants_enters)++;
	(shared_resources->shm_immigrants_balance)++;
	sem_post(semaphore_guard);

	PRINT_OUT("%d: IMM %d: enters: %d: %d: %d\n",
		(shared_resources->shm_interactions_counter)++,
		(identifi),
		(shared_resources->shm_immigrants_enters),
		(shared_resources->shm_immigrants_checks),
		(shared_resources->shm_immigrants_balance));

	sem_post(semaphore_noJudge);
	sem_wait(semaphore_mutex);

	sem_wait(semaphore_guard);
	(shared_resources->shm_immigrants_checks)++;
	sem_post(semaphore_guard);

	PRINT_OUT("%d: IMM %d: checks: %d: %d: %d\n",
		(shared_resources->shm_interactions_counter)++,
		(identifi),
		(shared_resources->shm_immigrants_enters),
		(shared_resources->shm_immigrants_checks),
		(shared_resources->shm_immigrants_balance));
	/**
	 * Davam vediet ze prebehli vsektky registracie aby mohla prebehnut cast s vydavanim rozhodnuti.  
	 * Samozrejme sudca musi byt v budove a zaroven NE == NC (pristahovalci, ktory vstupili a pristahovalci, ktory boli registrovany) 
	 */

	if ((shared_resources->shm_judge_entered == 1) && (shared_resources->shm_immigrants_enters) == (shared_resources->shm_immigrants_checks)) {
		sem_post(semaphore_allSignedIn); 
	}
	else {
		sem_post(semaphore_mutex);
	}

	sem_wait(semaphore_confirmed);
	PRINT_OUT("%d: IMM %d: wants certificate: %d: %d: %d\n",
		(shared_resources->shm_interactions_counter)++,
		(identifi),
		(shared_resources->shm_immigrants_enters),
		(shared_resources->shm_immigrants_checks),
		(shared_resources->shm_immigrants_balance));
	usleep(wait_time * 1000);

	PRINT_OUT("%d: IMM %d: got certificate: %d: %d: %d\n",
		(shared_resources->shm_interactions_counter)++,
		(identifi),
		(shared_resources->shm_immigrants_enters),
		(shared_resources->shm_immigrants_checks),
		(shared_resources->shm_immigrants_balance));

	sem_wait(semaphore_noJudge);
	sem_wait(semaphore_guard);
	(shared_resources->shm_immigrants_balance)--;
	sem_post(semaphore_guard);

	PRINT_OUT("%d: IMM %d: leaves: %d: %d: %d\n",
		(shared_resources->shm_interactions_counter)++,
		(identifi),
		(shared_resources->shm_immigrants_enters),
		(shared_resources->shm_immigrants_checks),
		(shared_resources->shm_immigrants_balance));
	sem_wait(semaphore_guard);
	(identifi)--;
	sem_post(semaphore_guard);
	sem_post(semaphore_noJudge);
}

void proc_judge(int tts_judge, int tt_judgement, int cnt_procend) {
	int judge; // JG time from input arguments
	int judgement; // JT time from input arguments
	if (tts_judge != 0) {
		judge = rand() % tts_judge; //time to generate next imigrant
	}
	judge = tts_judge;

	// 0 nemozem robit modulo 
	if (tts_judge != 0) { 
		judgement = rand() % tt_judgement; //time to generate next imigrant
	}
	judgement = tt_judgement;

	/**
	 * Ak neboli vsetky PI procesy sudcom spracovane opakuje sudca vstup a spracovava ziadosti 
	 */

	while (cnt_procend > 0) {
		usleep(judge * 1000);
		PRINT_OUT("%d: JUDGE: wants to enter\n", (shared_resources->shm_interactions_counter)++);
		sem_wait(semaphore_noJudge);
		PRINT_OUT("%d: JUDGE: enters: %d: %d: %d\n",
			(shared_resources->shm_interactions_counter)++,
			(shared_resources->shm_immigrants_enters),
			(shared_resources->shm_immigrants_checks),
			(shared_resources->shm_immigrants_balance));

		sem_wait(semaphore_guard);
		(shared_resources->shm_judge_entered) = 1;
		sem_post(semaphore_guard);
		// NE != NC
		if ((shared_resources->shm_immigrants_enters) > (shared_resources->shm_immigrants_checks)) {
			sem_wait(semaphore_allSignedIn);
			sem_wait(semaphore_mutex);
			PRINT_OUT("%d: JUDGE: waits for imm: %d: %d: %d\n",
				(shared_resources->shm_interactions_counter)++,
				(shared_resources->shm_immigrants_enters),
				(shared_resources->shm_immigrants_checks),
				(shared_resources->shm_immigrants_balance));
		}
		// NE == NC 
		if ((shared_resources->shm_immigrants_enters) == (shared_resources->shm_immigrants_checks)) {
			
			sem_post(semaphore_mutex);
			PRINT_OUT("%d: JUDGE: starts confirmation: %d: %d: %d\n",
				(shared_resources->shm_interactions_counter)++,
				(shared_resources->shm_immigrants_enters),
				(shared_resources->shm_immigrants_checks),
				(shared_resources->shm_immigrants_balance));
			usleep(judgement * 1000);
			sem_wait(semaphore_guard);
			(shared_resources->shm_immigrants_enters) = 0;
			(shared_resources->shm_immigrants_checks) = 0;
			sem_post(semaphore_guard);
			PRINT_OUT("%d: JUDGE: ends confirmation: %d: %d: %d\n",
				(shared_resources->shm_interactions_counter)++,
				(shared_resources->shm_immigrants_enters),
				(shared_resources->shm_immigrants_checks),
				(shared_resources->shm_immigrants_balance));
			sem_post(semaphore_confirmed);
		}
		
		usleep(judgement * 1000);
		PRINT_OUT("%d: JUDGE: leaves: %d: %d: %d\n",
			(shared_resources->shm_interactions_counter)++,
			(shared_resources->shm_immigrants_enters),
			(shared_resources->shm_immigrants_checks),
			(shared_resources->shm_immigrants_balance));
		sem_wait(semaphore_guard);
		(shared_resources->shm_judge_entered) = 0;
		sem_post(semaphore_guard);

	
	
	sem_post(semaphore_mutex);
	sem_post(semaphore_noJudge);
	cnt_procend = (cnt_procend - 1);
	}
	PRINT_OUT("%d: JUDGE: finishes\n", (shared_resources->shm_interactions_counter)++);
}

/**
 * @brief Cistenie prostriedkov po priebehu chodu skriptu
 */

void cleanup()
{
	sem_close(semaphore_print);
	sem_close(semaphore_noJudge);
	sem_close(semaphore_mutex);
	sem_close(semaphore_allSignedIn);
	sem_close(semaphore_confirmed);
	sem_close(semaphore_guard);
	sem_unlink(SEM_PRINT);
	sem_unlink(SEM_JUDGE_ENTER);
	sem_unlink(SEM_MUTEX);
	sem_unlink(SEM_ALLSIGIN);
	sem_unlink(SEM_CONF);
	shmdt(shared_resources);
	shmctl(shm_id, IPC_RMID, NULL);
	fclose(output_file);
}
