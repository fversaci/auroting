#include <string>
#include <iostream>
#include <vector>
#include <cstdio>
using namespace std;

int main(){

  //string inifile;
  string inifile(" -f altro.ini ");
  vector<string> confs;
  confs.push_back("Butterfly");
  // confs.push_back("Transposition");
  confs.push_back("TDTrans");
  // confs.push_back("Alterow");
  // confs.push_back("Uniform");
  // confs.push_back("Tornado");
  // confs.push_back("Bitcomplement");
  // confs.push_back("Permutation");

  vector<int> iter(confs.size(),0);
  int tot=0;
  for(int i=0; i<confs.size(); ++i){
    string cmdstr="./aurouting -u Cmdenv -x " + confs[i] + inifile + " | grep 'Number of runs:' | cut -c 17-";
    FILE *out=popen(cmdstr.c_str(), "r");
    int it=0;
    fscanf(out, "%d", &it);
    pclose(out);
    iter[i]=it;
    tot+=it;
  }

  // number of actual runs
  cout << "# " << tot << endl << endl;

  cout << ".PHONY: ";
  for (int i=0; i<tot; ++i)
    cout << " r" << i;
  for(int i=0; i<confs.size(); ++i){
    cout << " " << confs[i];
    cout << " c" << i;
  }
  cout << endl << endl;

  cout << "all: ";
  for(int i=0; i<confs.size(); ++i)
    cout << " " << confs[i];
  cout << endl << endl;

  int x=0;
  for(int i=0; i<confs.size(); ++i){
    cout << confs[i] << ": c" << i << " ";
    for(int j=0; j<iter[i]; ++j)
      cout << "r" << x++ << " ";
    cout << endl << endl << "c" << i << ":" << endl << "\trm altro-results/" << confs[i] << "*.sca" << endl << endl;
  }

  x=0;
  for(int i=0; i<iter.size(); ++i){
    for(int j=0; j<iter[i]; ++j){
      cout << "r" << x++ << ":" << endl;
      cout << "\t./aurouting -u Cmdenv -c " << confs[i] << " -r " << j << inifile << endl;
    }
  }

  return 0;
}
