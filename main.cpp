#include <QApplication>
#include <QMessageBox>
#include <QDebug>

#include "EvernusApplication.h"
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    try
    {
        Evernus::EvernusApplication app{argc, argv};

        try
        {
            Evernus::MainWindow mainWnd{app.getCharacterRepository(), app.getKeyRepository(), app.getAPIManager()};
            app.connect(&mainWnd, SIGNAL(keysChanged()), SLOT(fetchCharacters()));
            mainWnd.showAsSaved();

            return app.exec();
        }
        catch (const std::exception &e)
        {
            qCritical() << e.what();
            QMessageBox::critical(nullptr, QCoreApplication::translate("main", "Error"), e.what());
            throw;
        }
    }
    catch (...)
    {
        return 1;
    }
}
