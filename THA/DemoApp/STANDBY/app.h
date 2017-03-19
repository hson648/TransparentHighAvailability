typedef struct _engineer engineer_t;

typedef struct _manager{
        char manager_name[30];
        engineer_t *engineer;
} manager_t;

struct _engineer{
        char eng_name[30];
        manager_t *manager;
};

