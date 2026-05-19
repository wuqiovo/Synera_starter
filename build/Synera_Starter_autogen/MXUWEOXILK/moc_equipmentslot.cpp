/****************************************************************************
** Meta object code from reading C++ file 'equipmentslot.h'
**
** Created by: The Qt Meta Object Compiler version 69 (Qt 6.11.0)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include "../../../src/gui/equipmentslot.h"
#include <QtCore/qmetatype.h>

#include <QtCore/qtmochelpers.h>

#include <memory>


#include <QtCore/qxptype_traits.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'equipmentslot.h' doesn't include <QObject>."
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
struct qt_meta_tag_ZN17EquipmentSlotItemE_t {};
} // unnamed namespace

template <> constexpr inline auto EquipmentSlotItem::qt_create_metaobjectdata<qt_meta_tag_ZN17EquipmentSlotItemE_t>()
{
    namespace QMC = QtMocConstants;
    QtMocHelpers::StringRefStorage qt_stringData {
        "EquipmentSlotItem",
        "discardClicked",
        "",
        "slotIndex",
        "dragStarted",
        "QPointF",
        "scenePos",
        "dragMoved",
        "dragDropped"
    };

    QtMocHelpers::UintData qt_methods {
        // Signal 'discardClicked'
        QtMocHelpers::SignalData<void(int)>(1, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 3 },
        }}),
        // Signal 'dragStarted'
        QtMocHelpers::SignalData<void(int, QPointF)>(4, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 3 }, { 0x80000000 | 5, 6 },
        }}),
        // Signal 'dragMoved'
        QtMocHelpers::SignalData<void(int, QPointF)>(7, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 3 }, { 0x80000000 | 5, 6 },
        }}),
        // Signal 'dragDropped'
        QtMocHelpers::SignalData<void(int, QPointF)>(8, 2, QMC::AccessPublic, QMetaType::Void, {{
            { QMetaType::Int, 3 }, { 0x80000000 | 5, 6 },
        }}),
    };
    QtMocHelpers::UintData qt_properties {
    };
    QtMocHelpers::UintData qt_enums {
    };
    return QtMocHelpers::metaObjectData<EquipmentSlotItem, qt_meta_tag_ZN17EquipmentSlotItemE_t>(QMC::MetaObjectFlag{}, qt_stringData,
            qt_methods, qt_properties, qt_enums);
}
Q_CONSTINIT const QMetaObject EquipmentSlotItem::staticMetaObject = { {
    QMetaObject::SuperData::link<QGraphicsObject::staticMetaObject>(),
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17EquipmentSlotItemE_t>.stringdata,
    qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17EquipmentSlotItemE_t>.data,
    qt_static_metacall,
    nullptr,
    qt_staticMetaObjectRelocatingContent<qt_meta_tag_ZN17EquipmentSlotItemE_t>.metaTypes,
    nullptr
} };

void EquipmentSlotItem::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    auto *_t = static_cast<EquipmentSlotItem *>(_o);
    if (_c == QMetaObject::InvokeMetaMethod) {
        switch (_id) {
        case 0: _t->discardClicked((*reinterpret_cast<std::add_pointer_t<int>>(_a[1]))); break;
        case 1: _t->dragStarted((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QPointF>>(_a[2]))); break;
        case 2: _t->dragMoved((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QPointF>>(_a[2]))); break;
        case 3: _t->dragDropped((*reinterpret_cast<std::add_pointer_t<int>>(_a[1])),(*reinterpret_cast<std::add_pointer_t<QPointF>>(_a[2]))); break;
        default: ;
        }
    }
    if (_c == QMetaObject::IndexOfMethod) {
        if (QtMocHelpers::indexOfMethod<void (EquipmentSlotItem::*)(int )>(_a, &EquipmentSlotItem::discardClicked, 0))
            return;
        if (QtMocHelpers::indexOfMethod<void (EquipmentSlotItem::*)(int , QPointF )>(_a, &EquipmentSlotItem::dragStarted, 1))
            return;
        if (QtMocHelpers::indexOfMethod<void (EquipmentSlotItem::*)(int , QPointF )>(_a, &EquipmentSlotItem::dragMoved, 2))
            return;
        if (QtMocHelpers::indexOfMethod<void (EquipmentSlotItem::*)(int , QPointF )>(_a, &EquipmentSlotItem::dragDropped, 3))
            return;
    }
}

const QMetaObject *EquipmentSlotItem::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *EquipmentSlotItem::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_staticMetaObjectStaticContent<qt_meta_tag_ZN17EquipmentSlotItemE_t>.strings))
        return static_cast<void*>(this);
    return QGraphicsObject::qt_metacast(_clname);
}

int EquipmentSlotItem::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QGraphicsObject::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 4)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 4;
    }
    if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 4)
            *reinterpret_cast<QMetaType *>(_a[0]) = QMetaType();
        _id -= 4;
    }
    return _id;
}

// SIGNAL 0
void EquipmentSlotItem::discardClicked(int _t1)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 0, nullptr, _t1);
}

// SIGNAL 1
void EquipmentSlotItem::dragStarted(int _t1, QPointF _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 1, nullptr, _t1, _t2);
}

// SIGNAL 2
void EquipmentSlotItem::dragMoved(int _t1, QPointF _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 2, nullptr, _t1, _t2);
}

// SIGNAL 3
void EquipmentSlotItem::dragDropped(int _t1, QPointF _t2)
{
    QMetaObject::activate<void>(this, &staticMetaObject, 3, nullptr, _t1, _t2);
}
QT_WARNING_POP
