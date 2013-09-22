#define MAXLEN          128
#define MAXVARIABLES    20
#define READFBUF        128
#define YES             1
#define NO              0

#define PERROR(fmt, ...) do { if (error) { fprintf(stderr, "[ERROR] " fmt, __VA_ARGS__); return -1; }; } while(0)
#define INDEXLEN(a,b)       sizeof(a)/sizeof(b)
#define VA_NUM_ARGS(...) VA_NUM_ARGS_IMPL(__VA_ARGS__, 5,4,3,2,1)
#define VA_NUM_ARGS_IMPL(_1,_2,_3,_4,_5,N,...)
#define CLEAR(a, b)      do { bzero(a, sizeof(a) ) ; bzero(b, sizeof(b) ); } while(0)

int parseconfig(char *config);
const char *get_variable(const char *global, const char *variables);

