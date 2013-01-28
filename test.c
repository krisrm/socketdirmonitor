#include "dirapp.h"
#include "host.h"

void runAllTests();
void testDirDiff();
void testPortCheck();
void testHosts();
void fail();
void test();

static int testCount;
static int failCount;

const char* dirname;

int main(int argc, char** argv){
	if (argc > 1){
		dirname = argv[1];
	} else {
		die("Need a dirname\n");
	}

	runAllTests();
}

void runAllTests(){
	testDirDiff();
	testPortCheck();
	testHosts();
	printf("Failed %d of %d\n",failCount,testCount);
}

void testDirDiff(){
	dirdiff d;
	d.name = dirname;
	system("touch testdir/testfile2");
	system("touch testdir/testfile4");
	diffDirectory(&d);
	system("rm -f testdir/testfile2");
	system("echo 'morecontent' >> testdir/testfile");
	system("touch testdir/testfile");
	system("chmod u-x testdir/testfile4");
	system("touch testdir/testfile3");
	diffDirectory(&d);
	printDiffEntries(&d);
	system("rm -f testdir/testfile3");
	system("rm -f testdir/testfile4");
	diffDirectory(&d);
	printDiffEntries(&d);
	system("rm -f testdir/testfile");
}

void testPortCheck(){
	printf("Testing ports\n");
	int t;
		test();
	t = checkPort(-1);
	if (t == 1)
		fail();
	test();
	t = checkPort(0);
	if (t == 1)
		fail();
	t = checkPort(1024);
	test();
	if (t == 1)
		fail();

	t = checkPort(65536);
	test();
	if (t == 1)
		fail();
	t = checkPort(1025);
	test();
	if (t == 0)
		fail();
	printf("Done, failed %d\n",failCount);
}

void testHosts(){
	printf("Testing hosts\n");
	host hs[3];
	
	//clearHosts
	clearHosts(hs,3);
	for (int i = 0; i < 3; i++){
		test();
		if (hs[i].status != H_DISC){
			fail();
		}
	}

	//selectHostFD
	test();
	if (selectHostFD(hs,2,3) != -1)
		fail();

	hs[1].sockfd = 2;
	test();
	if (selectHostFD(hs,2,3) != 1)
		fail();

	hs[2].sockfd = 2;
	test();
	if (selectHostFD(hs,2,3) != 1)
		fail();


	//selectDiscHost
	test();
	if (selectDiscHost(hs,3) != 0)
		fail();
	
	test();
	hs[0].status = H_CONN;
	if (selectDiscHost(hs,3) != 1)
		fail();

	
	//activeHosts
	test();
	if (activeHosts(hs,3) != 1)
		fail();

	test();
	hs[1].status = H_INIT;
	if (activeHosts(hs,3) != 2)
		fail();

	printf("Done, failed %d\n",failCount);
}	
void test(){
	testCount++;
}
void fail(){
	failCount++;
	printf("Failed Test No. %d\n",testCount);
}
