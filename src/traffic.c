#include "common.h"
#include "ifinfo.h"
#include "misc.h"
#include "traffic.h"

void trafficmeter(char iface[], int sampletime)
{
	/* received bytes packets errs drop fifo frame compressed multicast */
	/* transmitted bytes packets errs drop fifo colls carrier compressed */
	uint64_t rx, tx, rxp, txp;
	IFINFO firstinfo;
	char buffer[256];

	/* less than 2 seconds doesn't produce good results */
	if (sampletime<2) {
		printf("Error: Time for sampling too short.\n");
		exit(EXIT_FAILURE);
	}

	/* read interface info and get values to the first list */
	if (!getifinfo(iface)) {
		printf("Error: Interface \"%s\" not available, exiting.\n", iface);
		exit(EXIT_FAILURE);
	}
	firstinfo.rx = ifinfo.rx;
	firstinfo.tx = ifinfo.tx;
	firstinfo.rxp = ifinfo.rxp;
	firstinfo.txp = ifinfo.txp;

	/* wait sampletime and print some nice dots so that the user thinks
	something is done :) */
	snprintf(buffer, 256, "Sampling %s (%d seconds average)", iface,sampletime);
	printf("%s", buffer);
	fflush(stdout);
	sleep(sampletime/3);
	printf(".");
	fflush(stdout);
	sleep(sampletime/3);
	printf(".");
	fflush(stdout);
	sleep(sampletime/3);
	printf(".");
	fflush(stdout);
	if ((sampletime/3)*3!=sampletime) {
		sleep(sampletime-((sampletime/3)*3));
	}

	cursortocolumn(1);
	eraseline();

	/* read those values again... */
	if (!getifinfo(iface)) {
		printf("Error: Interface \"%s\" not available, exiting.\n", iface);
		exit(EXIT_FAILURE);
	}

	/* calculate traffic and packets seen between updates */
	rx = countercalc(&firstinfo.rx, &ifinfo.rx);
	tx = countercalc(&firstinfo.tx, &ifinfo.tx);
	rxp = countercalc(&firstinfo.rxp, &ifinfo.rxp);
	txp = countercalc(&firstinfo.txp, &ifinfo.txp);

	/* show the difference in a readable format */
	printf("%"PRIu64" packets sampled in %d seconds\n", rxp+txp, sampletime);
	printf("Traffic average for %s\n", iface);
	printf("\n      rx     %s         %5"PRIu64" packets/s\n", gettrafficrate(rx, sampletime, 15), (uint64_t)(rxp/sampletime));
	printf("      tx     %s         %5"PRIu64" packets/s\n\n", gettrafficrate(tx, sampletime, 15), (uint64_t)(txp/sampletime));

}

void livetrafficmeter(char iface[32], int mode)
{
	/* received bytes packets errs drop fifo frame compressed multicast */
	/* transmitted bytes packets errs drop fifo colls carrier compressed */
	uint64_t rx, tx, rxp, txp, timespent, timeslept;
	uint64_t rxtotal, txtotal, rxptotal, txptotal;
	uint64_t rxpmin, txpmin, rxpmax, txpmax;
	uint64_t rxmin, txmin, rxmax, txmax;
	int ratewidth, ppswidth, paddingwidth;
	char buffer[256], buffer2[256];
	IFINFO previnfo;

	printf("Monitoring %s...    (press CTRL-C to stop)\n\n", iface);
	if (cfg.ostyle != 4) {
		printf("   getting traffic...");
		fflush(stdout);
	}

	/* enable signal trap */
	intsignal = 0;
	if (signal(SIGINT, sighandler) == SIG_ERR) {
		perror("signal");
		exit(EXIT_FAILURE);
	}

	/* set some defaults */
	rxtotal=txtotal=rxptotal=txptotal=rxpmax=txpmax=0;
	rxpmin=txpmin=rxmin=txmin=MAX64;
	rxmax=txmax=0;
	timeslept = 0;

	timespent = (uint64_t)time(NULL);

	/* read /proc/net/dev and get values to the first list */
	if (!getifinfo(iface)) {
		printf("Error: Interface \"%s\" not available, exiting.\n", iface);
		exit(EXIT_FAILURE);
	}

	ratewidth = 15;
	ppswidth = 5;
	paddingwidth = 8;

	/* narrow output mode */
	if (cfg.ostyle == 0) {
		ratewidth = 12;
		ppswidth = 3;
		paddingwidth = 4;
	}

	cursorhide();

	/* loop until user gets bored */
	while (intsignal==0) {

		timeslept = (uint64_t)time(NULL);

		/* wait 2 seconds for more traffic */
		sleep(LIVETIME);

		timeslept = (uint64_t)time(NULL) - timeslept;

		/* break loop without calculations because sleep was probably interrupted */
		if (intsignal) {
			break;
		}

		/* use values from previous loop if this isn't the first time */
		previnfo.rx = ifinfo.rx;
		previnfo.tx = ifinfo.tx;
		previnfo.rxp = ifinfo.rxp;
		previnfo.txp = ifinfo.txp;

		/* read those values again... */
		if (!getifinfo(iface)) {
			cursorshow();
			printf("Error: Interface \"%s\" not available, exiting.\n", iface);
			exit(EXIT_FAILURE);
		}

		/* calculate traffic and packets seen between updates */
		rx = countercalc(&previnfo.rx, &ifinfo.rx);
		tx = countercalc(&previnfo.tx, &ifinfo.tx);
		rxp = countercalc(&previnfo.rxp, &ifinfo.rxp);
		txp = countercalc(&previnfo.txp, &ifinfo.txp);

		/* update totals */
		rxtotal += rx;
		txtotal += tx;
		rxptotal += rxp;
		txptotal += txp;

		/* update min & max */
		if (rxmin>rx) {	rxmin = rx;	}
		if (txmin>tx) {	txmin = tx;	}
		if (rxmax<rx) {	rxmax = rx;	}
		if (txmax<tx) {	txmax = tx;	}
		if (rxpmin>rxp) { rxpmin = rxp;	}
		if (txpmin>txp) { txpmin = txp;	}
		if (rxpmax<rxp) { rxpmax = rxp;	}
		if (txpmax<txp) { txpmax = txp;	}

		/* show the difference in a readable format */
		if (mode == 0) {
			/* packets per second visible */
			snprintf(buffer, 128, "   rx: %s %*"PRIu64" p/s", gettrafficrate(rx, LIVETIME, ratewidth), ppswidth, (uint64_t)rxp/LIVETIME);
			snprintf(buffer2, 128, " %*s tx: %s %*"PRIu64" p/s", paddingwidth, " ", gettrafficrate(tx, LIVETIME, ratewidth), ppswidth, (uint64_t)txp/LIVETIME);
		} else {
			/* total transfer amount visible */
			snprintf(buffer, 128, "   rx: %s   %s", gettrafficrate(rx, LIVETIME, ratewidth), getvalue(0, rintf(rxtotal/(float)1024), 1, 1));
			snprintf(buffer2, 128, " %*s tx: %s   %s", paddingwidth, " ", gettrafficrate(tx, LIVETIME, ratewidth), getvalue(0, rintf(txtotal/(float)1024), 1, 1));
		}
		strncat(buffer, buffer2, 127);

		if (cfg.ostyle!=4 || !debug) {
			cursortocolumn(1);
			eraseline();
		}
		if (cfg.ostyle!=4) {
			printf("%s", buffer);
			fflush(stdout);
		} else {
			printf("%s\n", buffer);
		}
	}

	timespent = (uint64_t)time(NULL) - timespent - timeslept;
	cursorshow();

	printf("\n\n");

	/* print some statistics if enough time did pass */
	if (timespent>=10) {

		printf("\n %s  /  traffic statistics\n\n", iface);

		printf("                           rx         |       tx\n");
		printf("--------------------------------------+------------------\n");
		printf("  bytes              %s", getvalue(0, rintf(rxtotal/(float)1024), 15, 1));
		printf("  | %s", getvalue(0, rintf(txtotal/(float)1024), 15, 1));
		printf("\n");
		printf("--------------------------------------+------------------\n");
		printf("          max        %s", gettrafficrate(rxmax, LIVETIME, 15));
		printf("  | %s\n", gettrafficrate(txmax, LIVETIME, 15));
		printf("      average        %s", gettrafficrate(rxtotal, timespent, 15));
		printf("  | %s\n", gettrafficrate(txtotal, timespent, 15));
		printf("          min        %s", gettrafficrate(rxmin, LIVETIME, 15));
		printf("  | %s\n", gettrafficrate(txmin, LIVETIME, 15));
		printf("--------------------------------------+------------------\n");
		printf("  packets               %12"PRIu64"  |    %12"PRIu64"\n", rxptotal, txptotal);
		printf("--------------------------------------+------------------\n");
		printf("          max          %9"PRIu64" p/s  |   %9"PRIu64" p/s\n", rxpmax/LIVETIME, txpmax/LIVETIME);
		printf("      average          %9"PRIu64" p/s  |   %9"PRIu64" p/s\n", rxptotal/timespent, txptotal/timespent);
		printf("          min          %9"PRIu64" p/s  |   %9"PRIu64" p/s\n", rxpmin/LIVETIME, txpmin/LIVETIME);
		printf("--------------------------------------+------------------\n");

		if (timespent<=60) {
			printf("  time             %9"PRIu64" seconds\n", timespent);
		} else {
			printf("  time               %7.2f minutes\n", timespent/(float)60);
		}

		printf("\n");
	}

}
