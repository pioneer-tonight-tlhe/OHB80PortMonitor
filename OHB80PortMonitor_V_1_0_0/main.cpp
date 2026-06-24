#include "app.h"
#include "quiwidget.h"
#include "uidemo6.h"

int main(int argc, char *argv[])
{
#if (QT_VERSION >= QT_VERSION_CHECK(5,6,0))
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

    QApplication a(argc, argv);

    // 初始化App类
    if (!App::initialize()) {
        qCritical() << "Failed to initialize application";
        return -1;
    }
    
    a.setFont(QFont("Microsoft Yahei", 9));

    QUIWidget::setCode();
    QUIWidget::setTranslator(":/image/qt_zh_CN.qm");
    QUIWidget::setTranslator(":/image/widgets.qm");

    // 关闭时序：
    //   aboutToQuit → Scheduler::stop()（停后台任务）
    //   → a.exec() 返回 → qui 析构（UI 销毁，日志仍可用）
    //   → App::cleanup()（关闭日志、释放资源）
    int result;
    {
        QUIWidget qui;
        qui.setStyle(QUIWidget::Style_LightBlue);
        UIDemo6 *creator = new UIDemo6;
        creator->setDoubleClickMaximize(true);

        //设置主窗体
        qui.setMainWidget(creator);
        qui.setTitleBarVisible(false);
        qui.setMovable(true);

        QObject::connect(creator, SIGNAL(requestMinimize()), &qui, SLOT(on_btnMenu_Min_clicked()));
        QObject::connect(creator, SIGNAL(requestMaximizeOrRestore()), &qui, SLOT(on_btnMenu_Max_clicked()));
        QObject::connect(creator, SIGNAL(requestClose()), &qui, SLOT(on_btnMenu_Close_clicked()));


//设置标题
#ifdef demo
        // qui.setTitle("QUI皮肤生成器 演示版 (作者: 飞扬青云  QQ: 517216493)");
#else
        // qui.setTitle("QUI皮肤生成器 (作者: 飞扬青云  QQ: 517216493)");
#endif

        //设置标题文本居中
        qui.setAlignment(Qt::AlignCenter);

        //设置窗体可拖动大小
        qui.setSizeGripEnabled(true);

        //设置换肤下拉菜单可见
        qui.setVisible(QUIWidget::BtnMenu, true);

        //设置标题栏高度
        //qui.setTitleHeight(50);

        //设置按钮宽度
        //qui.setBtnWidth(50);

        //设置左上角图标-图形字体
        //qui.setIconMain(QChar(0xf099), 11);

        //设置左上角图标-图片文件
        //qui.setPixmap(QUIWidget::Lab_Ico, ":/main.ico");

        qui.show();

        result = a.exec();
    } // qui 析构：所有 UI 组件销毁完毕，日志系统仍可用

    // Phase 2: 关闭日志系统、释放资源（此时无后台任务、无 UI 组件）
    App::cleanup();
    return result;
}
