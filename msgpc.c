#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <stdlib.h>
#include <time.h>
#include <sys/wait.h>
#include <getopt.h>

typedef struct msgbuf
{
	long type;
	int value[2];
} MY_MSGBUF;

typedef struct configure
{
	int numsP;
	int worktimes;
	int numsC;
	int times;
	int delay;
} CONF_NODE;

/*Offer default configure*/
#define DEFAULT_P 3
#define DEFAULT_W 8
#define DEFAULT_C 3
#define DEFAULT_T 8
#define DEFAULT_D 4096
CONF_NODE myConfigure = {DEFAULT_P, DEFAULT_W, DEFAULT_C, DEFAULT_T, DEFAULT_D};

#define OPTMASK 1u

void usage(void);
int producer(int msg_empty, int msg_full, int msg_access, int producerID);
int consumer(int msg_empty, int msg_full, int msg_access, int consumerID);
/*Report the status of repo. When this function is running, the repo is unavailable to others.*/
int repoInfo(int msg_full, int msg_access);
/*Check the validity of customed configure.*/
int checkConfigure(void);
/*Show current configure.*/
int showConf(void);

int main(int argc, char *argv[])
{
	/*Define opt struct.*/
	static struct option pc_opts[] = {
		{"help", no_argument, NULL, 'h'},
		{"version", no_argument, NULL, 'v'},
		{"numsP", required_argument, NULL, 'p'},
		{"worktimes", required_argument, NULL, 'w'},
		{"numsC", required_argument, NULL, 'c'},
		{"times", required_argument, NULL, 't'},
		{"delay", required_argument, NULL, 'd'},
		{0, 0, 0, 0}};
	/*Handle the args.*/
	int opt;
	unsigned char detected = 0u;
	int opt_index = 0;
	while ((opt = getopt_long(argc, argv, "hvp:w:c:t:d:i:", pc_opts, &opt_index)) != -1)
	{
		switch (opt)
		{
		case 'h':
			detected |= (OPTMASK << 0);
			break;
		case 'v':
			detected |= (OPTMASK << 1);
			break;
		case 'p':
			detected |= (OPTMASK << 2);
			if (atoi(optarg))
				myConfigure.numsP = atoi(optarg);
			break;
		case 'w':
			detected |= (OPTMASK << 3);
			if (atoi(optarg))
				myConfigure.worktimes = atoi(optarg);
			break;
		case 'c':
			detected |= (OPTMASK << 4);
			if (atoi(optarg))
				myConfigure.numsC = atoi(optarg);
			break;
		case 't':
			detected |= (OPTMASK << 5);
			if (atoi(optarg))
				myConfigure.times = atoi(optarg);
			break;
		case 'd':
			detected |= (OPTMASK << 6);
			if (atoi(optarg))
				myConfigure.delay = atoi(optarg);
			break;
		case '?':
		default:
			fprintf(stderr, "Invalid args detected, program exiting\n");
			usage();
			exit(EXIT_FAILURE);
		}
	}
	/*Check the validity of arg combination.*/
	if (((detected & 0xFCu) && (detected & 0x03)) || ((detected & OPTMASK) && (detected & (OPTMASK << 1))))
	{
		fprintf(stderr, "Invalid combination of args detected, program exiting\n");
		usage();
		exit(EXIT_FAILURE);
	}
	else if (detected & (OPTMASK << 0))
	{
		usage();
		exit(EXIT_SUCCESS);
	}
	else if (detected & (OPTMASK << 1))
	{
		fprintf(stdout, "msgpc -- v1.1\n");
		exit(EXIT_SUCCESS);
	}
	else if (checkConfigure() == -1)
	{
		fprintf(stderr, "***********************************************************************************\n");
		fprintf(stderr, "Invalid customed configure value detected, program will run with default configure.\n");
		fprintf(stderr, "***********************************************************************************\n");
		myConfigure.numsP = DEFAULT_P;
		myConfigure.worktimes =  DEFAULT_W;
		myConfigure.numsC =  DEFAULT_C;
		myConfigure.times =  DEFAULT_T;
		showConf();
	}
	else
	{
		showConf();
	}
	/*Create msg queues.*/
	key_t repoKey = ftok(argv[0], 1);
	key_t emptyKey = ftok(argv[0], 2);
	key_t fullKey = ftok(argv[0], 3);
	int msgRepo = msgget(repoKey, IPC_CREAT | 0666);
	int msgEmpty = msgget(emptyKey, IPC_CREAT | 0666);
	int msgFull = msgget(fullKey, IPC_CREAT | 0666);
	/*Record the current pid.*/
	pid_t curr_pid = getpid();
	/*Offer the initial value of msg struct.*/
	MY_MSGBUF product = {curr_pid, {0, 0}};
	MY_MSGBUF repo = {curr_pid, {0, 26}};
	/*Init the msg that represent the status of repo.*/
	int index;
	int init_status = 0;
	for (index = 0; index < 26; index++)
	{
		product.value[0] = index;
		init_status += msgsnd(msgEmpty, &product, sizeof(product.value), 0);
	}
	if (init_status)
	{
		fprintf(stderr, "Failed to init the repo.\n");
	}
	else
	{
		fprintf(stdout, "Init the repo successfully!\n");
	}
	/*Reset the seed of rand()*/
	srand(time(NULL));
	/*create child processes.*/
	fprintf(stdout, "Preparing for chiled process...");
	pid_t pid_value;
	for (index = 0; index < myConfigure.numsP; index++)
	{
		pid_value = fork();
		if (pid_value < 0)
			fprintf(stderr, "\rfork error. Program exit!\n");
		else if (pid_value == 0)
		{
			for (int times = 0; times < myConfigure.worktimes; times++)
			{
				producer(msgEmpty, msgFull, msgRepo, index);
			}
			exit(EXIT_SUCCESS);
		}
		else
		{
			continue;
		}
	}
	for (index = 0; index < myConfigure.numsC; index++)
	{
		pid_value = fork();
		if (pid_value < 0)
			fprintf(stderr, "\rFork error. Program exit!\n");
		else if (pid_value == 0)
		{
			for (int ct = 0; ct < myConfigure.times; ct++)
			{
				consumer(msgEmpty, msgFull, msgRepo, index);
			}
			exit(EXIT_SUCCESS);
		}
		else
		{
			continue;
		}
	}
	/*Ensure the access of repo, starting the P-C.*/
	fprintf(stdout, "Preparation done!\n");
	msgsnd(msgRepo, &repo, sizeof(repo.value), 0);
	/*Check pid*/
	if (getpid() == curr_pid)
	{
		/*Wait for child processes.*/
		int status;
		while (wait(&status) != -1)
			continue;
		/*Report the basic status of the repo*/
		repoInfo(msgFull, msgRepo);
		/*Destory the msg queue.*/
		struct msqid_ds tmpbuf;
		msgctl(msgEmpty, IPC_RMID, &tmpbuf);
		msgctl(msgFull, IPC_RMID, &tmpbuf);
		msgctl(msgRepo, IPC_RMID, &tmpbuf);
	}
	return 0;
}

void usage(void)
{
	fprintf(stdout, "Usage: \tmsgpc [-p <num>] [-w <num>] [-c <num>] [-t <num>] [-d <usecs>]\n");
	fprintf(stdout, "\tOr: msgpc [-v / --version]\n");
	fprintf(stdout, "\tOr: msgpc [-h / --help]\n\n");
	fprintf(stdout, "\tOptions:\n");
	fprintf(stdout, "\t\t [-p <num>] or [--numsP=<num>]:\tThe number of producer.\n");
	fprintf(stdout, "\t\t [-w <num>] or [--worktimes=<num>]:\tThe working times of each producer.\n");
	fprintf(stdout, "\t\t [-c <num>] or [--numsC=<num>]:\tThe number of consumer.\n");
	fprintf(stdout, "\t\t [-t <num>] or [--times=<num>]:\tThe purching times of each consumer.\n");
	fprintf(stdout, "\t\t [-d <usecs>] or [--delay=<secs>]:\tThe max delay time of each character (microseconds).\n");
	fprintf(stdout, "\t\t [-v] or [--version]:\tShow help info.\n");
	fprintf(stdout, "\t\t [-h] or [--help]:\tShow help info.\n");
}

int producer(int msg_empty, int msg_full, int msg_access, int producerID)
{
	MY_MSGBUF product = {0, {0, 0}};
	MY_MSGBUF repo = {0, {0, 26}};
	/*Record current pid.*/
	pid_t curr_pid = getpid();
	/*Check the empty units.*/
	if (msgrcv(msg_empty, &product, sizeof(product.value), 0, 0) == -1)
		fprintf(stderr, "\rPid %d: producer %d failed to get repo info.\n", curr_pid, producerID);
	/*Check the access status of the repo.*/
	if (msgrcv(msg_access, &repo, sizeof(repo.value), 0, 0) == -1)
		fprintf(stderr, "\rPid %d: producer %d failed to get access info of repo.\n", curr_pid, producerID);
	/*Start producing.*/
	product.type = curr_pid;
	product.value[1] = rand() % 1024;
	fprintf(stdout, "\rPid %d: producer %d completed the production work. { product id: %d, content: %d }\n ", curr_pid, producerID, product.value[0], product.value[1]);
	/*Wait a few microseconds, for running effects.*/
	unsigned randUsec = (unsigned)(rand() % myConfigure.delay);
	usleep(randUsec);
	/*Release the access right of repo.*/
	repo.type = curr_pid;
	if (msgsnd(msg_access, &repo, sizeof(repo.value), 0) == -1)
		fprintf(stderr, "\rPid %d: producer %d failed to release the access right of repo.\n", curr_pid, producerID);
	/*Update the repo info.*/
	if (msgsnd(msg_full, &product, sizeof(product.value), 0) == -1)
		fprintf(stderr, "\rPid %d: producer %d failed to update the repo info.\n", curr_pid, producerID);
}

int consumer(int msg_empty, int msg_full, int msg_access, int consumerID)
{
	MY_MSGBUF product = {0, {0, 0}};
	MY_MSGBUF repo = {0, {0, 26}};
	/*Record current pid.*/
	pid_t curr_pid = getpid();
	/*Check the available units.*/
	if (msgrcv(msg_full, &product, sizeof(product.value), 0, 0) == -1)
		fprintf(stderr, "\rPid %d: consumer %d failed to get repo info.\n", curr_pid, consumerID);
	/*Check the access status of the repo.*/
	if (msgrcv(msg_access, &repo, sizeof(repo.value), 0, 0) == -1)
		fprintf(stderr, "\rPid %d: consumer %d failed to get access info of repo.\n", curr_pid, consumerID);
	/*Start consuming.*/
	fprintf(stdout, "\rPid %d: consumer %d get a product from the repo. { Product id: %d, content: %d }\n", curr_pid, consumerID, product.value[0], product.value[1]);
	product.value[1] = 0;
	repo.type = curr_pid;
	/*Wait a few microseconds, for running effects.*/
	unsigned randUsec = (unsigned)(rand() % myConfigure.delay);
	usleep(randUsec);
	/*Release the access right of repo.*/
	if (msgsnd(msg_access, &repo, sizeof(repo.value), 0) == -1)
		fprintf(stderr, "\rPid %d: consumer %d failed to release the access right of repo.\n", curr_pid, consumerID);
	/*Update the repo info.*/
	if (msgsnd(msg_empty, &product, sizeof(product.value), 0) == -1)
		fprintf(stderr, "\rPid %d: consumer %d failed to update the repo info.\n", curr_pid, consumerID);
}
/*Report the status of repo. When this function is running, the repo is unavailable to others.*/
int repoInfo(int msg_full, int msg_access)
{
	MY_MSGBUF repo = {0, {0, 26}};
	/*Record current pid.*/
	pid_t curr_pid = getpid();
	/*Check the access status of the repo.*/
	if (msgrcv(msg_access, &repo, sizeof(repo.value), 0, 0) == -1)
		fprintf(stderr, "\rPid %d: administor failed to get access info of repo.\n", curr_pid);
	/*Get the basic info of the queue.*/
	fprintf(stdout, "\rChecking the current status of the repo...\n");
	struct msqid_ds tmpbuf;
	msgctl(msg_full, IPC_STAT, &tmpbuf);
	msgqnum_t nums = tmpbuf.msg_qnum;
	fprintf(stdout, "\rDone! %lu %s left in the repo.\n", nums, (nums > 1) ? "products" : "product");
	/*Release the access right of repo.*/
	if (msgsnd(msg_access, &repo, sizeof(repo.value), 0) == -1)
		fprintf(stderr, "\rPid %d: administaor failed to release the access right of repo.\n", curr_pid);
}
/*Check the validity of customed configure.*/
int checkConfigure(void)
{
	int total = myConfigure.numsP * myConfigure.worktimes;
	int comsumed = myConfigure.numsC * myConfigure.times;
	return (total < comsumed || (total - comsumed) > 26) ? -1 : 0;
}

/*Show current configure.*/
int showConf()
{
	fprintf(stdout, "Args checked! Here is the configure: \n");
	fprintf(stdout, "\t%d producers, each of them will work %d times.\n ", myConfigure.numsP, myConfigure.worktimes);
	fprintf(stdout, "\t%d consumers, each of them will purchase %d times.\n", myConfigure.numsC, myConfigure.times);
	fprintf(stdout, "\tThe max delay value is %d microseconds.\n", myConfigure.delay);
}