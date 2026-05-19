/****************************************************************************
** Meta object code from reading C++ file 'game.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/core/game.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'game.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 69
#error "This file was generated using the moc from 6.11.0. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

#ifndef Q_CONSTINIT
#define Q_CONSTINIT
#endif

QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
QT_WARNING_DISABLE_GCC("-Wuseless-cast")
namespace {
struct qt_meta_tag_ZN4GameE_t {};
} // unnamed namespace

template <> constexpr inline auto Game::qt_create_metaobjectdata<qt_meta_tag_ZN4GameE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "Game",
        "playerInfoChanged",
        "",
        "enemyInfoChanged",
        "stageRoundChanged",
        "battleStateChanged",
        "inBattle",
        "gameOver",
        "enemyDefeated",
        "handleEqDiscardClicked",
        "slotIndex",
        "handleEqDragStarted",
        "QPointF",
        "scenePos",
        "handleEqDragMoved",
        "handleEqDragDropped"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'playerInfoChanged'
        QtMocHelpers::SignalData<void()>(1, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'enemyInfoChanged'
        QtMocHelpers::SignalData<void()>(3, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'stageRoundChanged'
        QtMocHelpers::SignalData<void()>(4, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'battleStateChanged'
        QtMocHelpers::SignalData<void(bool)>(5, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Bool, 6 },
        }}),
        // Signal 'gameOver'
        QtMocHelpers::SignalData<void()>(7, 2, QMC::AccessPublic, QMetaType::Void),
        // Signal 'enemyDefeated'
        QtMocHelpers::SignalData<void()>(8, 2, QMC::AccessPublic, QMetaType::Void),
        // Slot 'handleEqDiscardClicked'
        QtMocHelpers::SlotData<void(int)>(9, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 10 },
        }}),
        // Slot 'handleEqDragStarted'
        QtMocHelpers::SlotData<void(int, QPointF)>(11, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 10 }, { 0x80000000 | 12, 13 },
        }}),
        // Slot 'handleEqDragMoved'
        QtMocHelpers::SlotData<void(int, QPointF)>(14, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 10 }, { 0x80000000 | 12, 13 },
        }}),
        // Slot 'handleEqDragDropped'
        QtMocHelpers::SlotData<void(int, QPointF)>(15, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 10 }, { 0x80000000 | 12, 13 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<Game, qt_meta_tag_ZN4GameE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject Game::staticMetaObject = { {
    QMetaObject::SuperData::link<QObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN4GameE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN4GameE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN4GameE_t>.metaTypes,
    nullptr
} };

void Game::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<Game *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->playerInfoChanged(); break;
        case 1: _t->enemyInfoChanged(); break;
        case 2: _t->stageRoundChanged(); break;
        case 3: _t->battleStateChanged((*reinterpret_cast<std::add_pointer_t<bool>>(_a[1]))); break;
        case 4: _t->gameOver(); break;
        case 5: _t->enemyDefeated(); break;
        case 6: _t->handleEqDiscardClicked((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 7: _t->handleEqDragStarted((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QPointF>>(_a[2]))); break;
        case 8: _t->handleEqDragMoved((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QPointF>>(_a[2]))); break;
        case 9: _t->handleEqDragDropped((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QPointF>>(_a[2]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (Game::*)()>(_a, &Game::playerInfoChanged, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (Game::*)()>(_a, &Game::enemyInfoChanged, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (Game::*)()>(_a, &Game::stageRoundChanged, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (Game::*)(bool )>(_a, &Game::battleStateChanged, 3))
            return;
        if (QtMocHelpers::indexOfMethod<void (Game::*)()>(_a, &Game::gameOver, 4))
            return;
        if (QtMocHelpers::indexOfMethod<void (Game::*)()>(_a, &Game::enemyDefeated, 5))
            return;
    }
}

const QMetaObject *Game::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *Game::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN4GameE_t>.strings))
        return static_cast<void*>(this);
    return QObject::qt_metacast(_clname);
}

int Game::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 10)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 10;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 10)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 10;
    }
    return _id;
}

// SIGNAL 0
void Game::playerInfoChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}

// SIGNAL 1
void Game::enemyInfoChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 1, nullptr);
}

// SIGNAL 2
void Game::stageRoundChanged()
{
    QMetaObject::activate(this, &staticMetaObject, 2, nullptr);
}

// SIGNAL 3
void Game::battleStateChanged(bool _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1);
}

// SIGNAL 4
void Game::gameOver()
{
    QMetaObject::activate(this, &staticMetaObject, 4, nullptr);
}

// SIGNAL 5
void Game::enemyDefeated()
{
    QMetaObject::activate(this, &staticMetaObject, 5, nullptr);
}
QT_WARNING_POP
