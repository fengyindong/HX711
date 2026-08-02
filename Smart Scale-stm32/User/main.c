#include "app.h"

/*
 * 主函数只负责启动应用并持续调度。
 * 各外设和业务逻辑均封装在独立模块中，便于单独测试和替换。
 */
int main(void)
{
    App_Init();
    for (;;) {
        App_Run();
    }
}
