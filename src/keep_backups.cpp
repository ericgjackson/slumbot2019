#include <stdio.h>
#include <stdlib.h>
#include <string.h>   // strcpy()
#include <sys/wait.h> // waitpid()
#include <unistd.h>   // vfork

#include <string>
#include <vector>

#include "io.h"
#include "split.h"

using std::string;
using std::vector;

// 2019-06-13 16:31:28    1187056 regrets.x.0.0.0.0.p0.i
// 2019-06-13 16:31:28    1187732 regrets.x.0.0.0.0.p1.i
// 2019-06-13 16:31:28 69054405888 regrets.x.0.0.1.0.p0.c
// 2019-06-13 16:31:28 69054405888 regrets.x.0.0.1.0.p1.c
// 2019-06-13 16:31:28 2233180000 regrets.x.0.0.2.0.p0.s
// 2019-06-13 16:31:28 2233180000 regrets.x.0.0.2.0.p1.s
// 2019-06-13 16:31:28 3839620000 regrets.x.0.0.3.0.p0.s
// 2019-06-13 16:32:34 3839620000 regrets.x.0.0.3.0.p1.s
// 2019-06-13 16:32:34    1187056 sumprobs.x.0.0.0.0.p0.i
// 2019-06-13 16:32:34    1187732 sumprobs.x.0.0.0.0.p1.i
// 2019-06-13 16:32:34 276217623552 sumprobs.x.0.0.1.0.p0.i
// 2019-06-13 16:33:39 276217623552 sumprobs.x.0.0.1.0.p1.i
// 2019-06-13 16:35:53 4466360000 sumprobs.x.0.0.2.0.p0.i
// 2019-06-13 16:40:13 4466360000 sumprobs.x.0.0.2.0.p1.i
// 2019-06-13 16:44:30 7679240000 sumprobs.x.0.0.3.0.p0.i
// 2019-06-13 16:51:52 7679240000 sumprobs.x.0.0.3.0.p1.i

const char *kDir = "/data/poker2019/cfr/holdem.2.b.13.4.3.big4sym.tcfrqcss";

static long long int TargetSize(int st, bool regrets, int p) {
  if (st == 0) {
    // Preflop regrets and sumprobs are the same size, but P0 and P1 differ
    if (p == 0) {
      return 1187056LL;
    } else {
      return 1187732LL;
    }
  } else if (st == 1) {
    if (regrets) {
      return 69054405888LL;
    } else {
      return 276217623552LL;
    }
  } else if (st == 2) {
    if (regrets) {
      return 2233180000LL;
    } else {
      return 4466360000LL;
    }
  } else {
    if (regrets) {
      return 3839620000LL;
    } else {
      return 7679240000LL;
    }
  }
}

static bool Ready(int it) {
  char buf[500];
  for (int st = 0; st <= 3; ++st) {
    for (int r = 0; r <= 1; ++r) {
      for (int p = 0; p <= 1; ++p) {
	if (r) {
	  sprintf(buf, "%s/regrets.x.0.0.%i.%i.p%i.%c", kDir, st, it, p,
		  st == 1 ? 'c' : (st >= 2 ? 's' : 'i'));
	} else {
	  sprintf(buf, "%s/sumprobs.x.0.0.%i.%i.p%i.i", kDir, st, it, p);
	}
	if (! FileExists(buf)) return false;
	long long int target_size = TargetSize(st, r, p);
	if (FileSize(buf) != target_size) return false;
      }
    }
  }
  return true;
}

// Uses vfork() and execvp() to fork off a child process to copy the files to S3.
// Unfortunate that we have to hard code the location of aws (/usr/bin).
static void Backup(void) {
  int pid = vfork();
  if (pid == 0) {
    // Child
    char const *binary = "/usr/bin/aws";
    char const * newargv[] = { binary, "s3", "cp", kDir, "s3://slumbot2019cfr", "--recursive",
			       "--quiet", NULL };
    execvp(binary, (char * const *)newargv);
    fprintf(stderr, "Failed to execvp aws s3 cp process\n");
    exit(-1);
  }
  int return_status;
  waitpid(pid, &return_status, 0);
  if (return_status != 0) {
    fprintf(stderr, "aws s3 cp failed\n");
    exit(-1);
  }
}

static bool BackupDone(int target_it) {
  char cmd[500], output[500];
  strcpy(cmd, "/usr/bin/aws s3 ls s3://slumbot2019cfr");
  FILE *fp = popen(cmd, "r");
  vector<string> lines;
  while (fgets(output, sizeof(output), fp)) {
    string line = output;
    lines.push_back(line);
  }
  int num_lines = lines.size();
  int num_done = 0;
  vector<string> comps1, comps2;
  for (int i = 0; i < num_lines; ++i) {
    const string &line = lines[i];
    Split(line.c_str(), ' ', false, &comps1);
    if (comps1.size() != 4) {
      fprintf(stderr, "Not 4 components in %s\n", line.c_str());
      exit(-1);
    }
    const string &sz_str = comps1[2];
    const string &fn = comps1[3];
    long long int file_size;
    if (sscanf(sz_str.c_str(), "%lli", &file_size) != 1) {
      fprintf(stderr, "Couldn't parse file size: %s\n", line.c_str());
      exit(-1);
    }
    Split(fn.c_str(), '.', false, &comps2);
    if (comps2.size() != 8) {
      fprintf(stderr, "Not 8 components in file name in %s\n", line.c_str());
      exit(-1);
    }
    int it, st, p;
    bool regrets;
    if (sscanf(comps2[5].c_str(), "%i", &it) != 1) {
      fprintf(stderr, "Couldn't parse iteration from file name in %s\n", line.c_str());
      exit(-1);
    }
    if (target_it == it) {
      if (sscanf(comps2[4].c_str(), "%i", &st) != 1) {
	fprintf(stderr, "Couldn't parse street from file name in %s\n", line.c_str());
	exit(-1);
      }
      if (comps2[0] == "regrets") {
	regrets = true;
      } else if (comps2[0] == "sumprobs") {
	regrets = false;
      } else {
	fprintf(stderr, "Not regrets or sumprobs?!?  %s\n", line.c_str());
	exit(-1);
      }
      if (comps2[6] == "p0") {
	p = 0;
      } else if (comps2[6] == "p1") {
	p = 1;
      } else {
	fprintf(stderr, "Not p0 or p1?!?  %s\n", line.c_str());
	exit(-1);
      }
      long long int target_size = TargetSize(st, regrets, p);
      if (target_size == file_size) ++num_done;
    }
  }
  return num_done == 16;
}

static void EmptyDirectory(void) {
  int pid = vfork();
  if (pid == 0) {
    // Child
    // We delete the directory as well as the contents.  run_tcfr will recreate it when it
    // checkpoints the next iteration.
    char const *binary = "/usr/bin/rm";
    char const * newargv[] = { binary, "-rf", kDir, NULL };
    execvp(binary, (char * const *)newargv);
    fprintf(stderr, "Failed to execvp rm -rf process\n");
    exit(-1);
  }
  int return_status;
  waitpid(pid, &return_status, 0);
  if (return_status != 0) {
    fprintf(stderr, "rm -rf failed\n");
    exit(-1);
  }
}

static void Usage(const char *prog_name) {
  fprintf(stderr, "USAGE: %s <it>\n", prog_name);
  exit(-1);
}

int main(int argc, char *argv[]) {
  if (argc != 2) Usage(argv[0]);
  int it;
  if (sscanf(argv[1], "%i", &it) != 1) Usage(argv[0]);
  while (true) {
    if (Ready(it)) {
      Backup();
      while (true) {
	if (BackupDone(it)) {
	  EmptyDirectory();
	  ++it;
	  break;
	} else {
	  sleep(60);
	}
      }
    } else {
      sleep(60);
    }
  }
}
