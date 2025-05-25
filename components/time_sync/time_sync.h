#ifndef _TIME_SYNC_H_
#define _TIME_SYNC_H_


#ifdef __cplusplus
extern "C" {
#endif
#include <stdbool.h>

void time_sync_start(void);  // 启动同步，阻塞等待同步完成
void time_sync_task(void* pvParameters);   // 任务入口函数
bool is_time_synced();

#ifdef __cplusplus
}
#endif

#endif // _TIME_SYNC_H_
