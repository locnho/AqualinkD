#ifndef AQ_FILESYSTEM_H_
#define AQ_FILESYSTEM_H_

#if MG_TLS > 0
  #define MAX_PEM_SIZE (64 * 1024)  // 64KB limit
  char* read_pem_file(const char* fmt, ...);
#endif

FILE *aq_open_file( char *filename, bool *ro_root, bool* created_file);
bool aq_close_file(FILE *file, bool ro_root);
bool copy_file(const char *source_path, const char *destination_path);
bool run_aqualinkd_upgrade(uint8_t type);



#endif //AQ_FILESYSTEM_H_