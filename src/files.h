#ifndef _FILES_H_
#define _FILES_H_

#include <string>

class Files {
public:
  static void Init(void);
  static const char *OldCFRBase(void);
  static const char *NewCFRBase(void);
  static const char *StaticBase(void);
private:
  static std::string old_cfr_base_;
  static std::string new_cfr_base_;
  static std::string static_base_;
};

#endif
