
#include <stdio>

void print_usage() {
  fprintf(stderr, "./test_orderings <composite> <1> <2>...");
}



int main(int argc, char **argv) {
  if (argc < 4) {
    print_usage();
    return 1;
  }

  struct shared_state *composite = shared_reader(argv[1]);
  std::vector<struct shared_state> iters;
  std::vector<char*> vals;

  for (int i = 2; i < argc; ++i) {
    iters.push_back(shared_reader(argv[i]));
  }


}
