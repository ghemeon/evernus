#pragma once

#include <memory>

#include <QApplication>
#include <QSqlDatabase>

#include "CharacterRepository.h"
#include "KeyRepository.h"

namespace Evernus
{
    class EvernusApplication
        : public QApplication
    {
    public:
        EvernusApplication(int &argc, char *argv[]);
        virtual ~EvernusApplication() = default;

        const KeyRepository &getKeyRepository() const noexcept;
        const CharacterRepository &getCharacterRepository() const noexcept;

    private:
        QSqlDatabase mMainDb;

        std::unique_ptr<KeyRepository> mKeyRepository;
        std::unique_ptr<CharacterRepository> mCharacterRepository;

        void createDb();
        void createDbSchema();
    };
}
