#ifndef __APP_H
#define __APP_H

/* 初始化整个电子秤应用及其所有硬件模块。 */
void App_Init(void);
/* 执行一次应用任务，main()需要在无限循环中持续调用。 */
void App_Run(void);

#endif
