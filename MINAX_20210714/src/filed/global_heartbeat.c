#include "bacula.h"
#include "filed.h"

extern pthread_t global_heartbeat_id;
static BSOCK *dir;
extern void *handle_director_request(BSOCK *dir);
DIRRES *director;
extern CLIENT *me;

extern "C" void *global_dir_heartbeat_thread(void *arg)
{
    LockRes();
    director = (DIRRES *)GetNextRes(R_DIRECTOR, NULL);
    UnlockRes();
    
    char * name = me->hdr.name;
    const char * _hello = "*Hello DIRD* ";
    char * heartbeat_cmd = (char *)malloc(strlen(name) + strlen(_hello) + 1);
    strcpy(heartbeat_cmd,_hello);
    strcat(heartbeat_cmd,name);
    //time_t last_heartbeat = time(NULL);
redo:
    dir = new_bsock();
    if (!dir->connect(NULL, 5, 15, 5, "Director daemon", director->dirinfo.address,NULL, director->dirinfo.DIRport, 0)) {
        dir->destroy();
        dir = NULL;
        return -1;
    }
    dir->fsend(heartbeat_cmd);
    //main loop
    while (!dir->is_stop()) {
        // time_t now;
        // now = time(NULL);
        // if ((now - last_heartbeat) >= 5) {
        //     //dir->fsend(heartbeat_cmd);
        //     if (dir->is_stop()) {
        //         break;
        //     }
        //     last_heartbeat = now;
        // }
        if (dir->recv() > 0) {
            if (strncmp(dir->msg, "PONG", 4) != 0) {
                if (strncmp(dir->msg, "Hello ", 5) == 0) {
                    pthread_create(&global_heartbeat_id, NULL, handle_director_request, dir);
                    dir = NULL;
                    bmicrosleep(5, 0);                  
                    goto redo;
                }
            }
        }
    }
}

void start_golbal_dir_heartbeat()
{
    if (me->heartbeat_interval > 0) {
        dir = new_bsock();
        pthread_create(&global_heartbeat_id, NULL, global_dir_heartbeat_thread, NULL);
    }
}

void stop_global_dir_heartbeat()
{
    if (me->heartbeat_interval > 0) {
        pthread_kill(global_heartbeat_id, TIMEOUT_SIGNAL);
    }
}