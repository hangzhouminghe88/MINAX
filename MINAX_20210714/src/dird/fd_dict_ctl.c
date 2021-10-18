#include "bacula.h"
#include "dird.h"
#include "dict.h"

extern pthread_mutex_t fd_ip_mutex;
extern dict *fd_ip_dict;
static pthread_t fd_dict_ctrl_thread_id;

extern "C" void fd_dict_ctrl_thread(void *arg)
{
    time_t last_heartbeat = time(NULL);
    time_t now;
    BSOCK *user;
    while (true)
    {
        now = time(NULL);
        if ((now - last_heartbeat) >= 15) {
            dictEntry *key;
            dictIterator *iter;
            P(fd_ip_mutex);
            iter = dictGetSafeIterator(fd_ip_dict);
            while((key = dictNext(iter)) != NULL) {
                user = (BSOCK *) dictGetVal(key);
                if(user->is_stop())
                {
                    Dmsg1(50, _("Client=%s is closed,remove!!!\n"), user->host());
                    dictDelete(fd_ip_dict, key);
                    free_bsock(user);
                }
                else{                  
                    user->fsend("PONG");
                    //user->recv();
                }   
            }
            last_heartbeat = now;
            dictReleaseIterator(iter);
            V(fd_ip_mutex);
            user = NULL;
        }
    }
}

void start_fd_dict_ctrl_thread()
{
    pthread_create(&fd_dict_ctrl_thread_id, NULL, fd_dict_ctrl_thread, NULL);
}

void stop_fd_dict_ctrl_thread()
{
    pthread_kill(fd_dict_ctrl_thread_id, TIMEOUT_SIGNAL);
}