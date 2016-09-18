//--------------------------------------------------------------------------
//                  Config.cpp - engine option-setting API
//                           -------------------
//  copyright            : (C) 2016 by Lucian Landry
//  email                : lucian_b_landry@yahoo.com
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
//
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU Lesser General Public License as
//   published by the Free Software Foundation; either version 2.1 of the
//   License, or (at your option) any later version.
//
//--------------------------------------------------------------------------

#include <algorithm> // std::find
#include <assert.h>
#include "Config.h"

const char *const Config::MaxDepthSpin = "limits/maxDepth";
const char *const Config::MaxDepthDescription =
    "Max depth engine may search.  0 implies 'no limit'.";

const char *const Config::MaxMemorySpin = "limits/maxMemory";
const char *const Config::MaxMemoryDescription =
    "Max cumulative size of transposition table + other adjustable caches (in MiB).";

const char *const Config::MaxNodesSpin = "limits/maxNodes";
const char *const Config::MaxNodesDescription =
    "Max nodes engine may search.  0 implies 'no limit'.";

const char *const Config::MaxThreadsSpin = "limits/maxThreads";
const char *const Config::MaxThreadsDescription =
    "Max threads engine may use to search.";

const char *const Config::RandomMovesCheckbox = "randomMoves";
const char *const Config::RandomMovesDescription =
    "True iff engine should randomize moves.";

const char *const Config::CanResignCheckbox = "canResign";
const char *const Config::CanResignDescription =
    "True iff engine may resign.";

const char *const Config::HistoryWindowSpin = "historyWindow";
const char *const Config::HistoryWindowDescription =
    "History heuristic (0 -> disabled, 1 -> killer moves, etc.)";

const char *Config::ErrorString(Config::Error error) const
{
    switch (error)
    {
        case Error::None:
            return "No error";
        case Error::NotFound:
            return "Item not found";
        case Error::WrongType:
            return "Item has wrong type for this method";
        case Error::InvalidValue:
            return "Value out of range, or not a valid choice";
        case Error::AlreadyExists:
            return "Item already exists";
        default:break;
    }
    assert(0); // All cases should be covered
    return ""; // Should not reach here
}

void Config::CheckboxItem::SetValue(bool value)
{
    if (this->value != value)
    {
        this->value = value;
        callback(*this);
    }
}

Config::Error Config::SpinItem::SetValue(int value)
{
    if (value < min || value > max)
        return Error::InvalidValue;
    if (this->value != value)
    {
        this->value = value;
        callback(*this);
    }
    return Error::None;
}

void Config::SpinItem::SetValueClamped(int value)
{
    value = std::max(value, min);
    value = std::min(value, max);
    
    if (this->value != value)
    {
        this->value = value;
        callback(*this);
    }
}

Config::Error Config::ComboItem::SetValue(const std::string &value)
{
    if (std::find(choices.begin(), choices.end(), value) == choices.end())
        return Error::InvalidValue;
    if (this->value != value)
    {
        this->value = value;
        callback(*this);
    }
    return Error::None;
}

void Config::StringItem::SetValue(const std::string &value)
{
    if (this->value != value)
    {
        this->value = value;
        callback(*this);
    }
}

void Config::ButtonItem::SetValue()
{
    callback(*this);
}

Config::~Config()
{
    // We should not leak the Items we copied.
    for (auto &iter : itemMap)
    {
        delete iter.second;
        iter.second = nullptr;
    }
}

Config::Error Config::Register(const Item &item)
{
    if (itemMap.find(item.Name()) != itemMap.end())
        return Error::AlreadyExists;

    itemMap[item.Name()] = item.Clone();
    return Error::None;
}

const Config::Item *Config::ItemAt(int idx) const
{
    for (const auto &iter : itemMap)
    {
        if (--idx < 0)
            return iter.second;
    }
    return nullptr;
}

Config::Item *Config::ItemAt(const std::string &name) const
{
    auto iter = itemMap.find(name);
    return iter != itemMap.end() ? iter->second : nullptr;
}

Config::Error Config::SetCheckbox(const std::string &name, bool value)
{
    Item *item = ItemAt(name);

    if (item == nullptr)
        return Error::NotFound;
        
    CheckboxItem *cItem = dynamic_cast<CheckboxItem *>(item);

    if (cItem == nullptr)
        return Error::WrongType;

    cItem->SetValue(value);
    return Error::None;
}

Config::Error Config::ToggleCheckbox(const std::string &name)
{
    const Config::CheckboxItem *cbItem = CheckboxItemAt(name);
    return
        cbItem == nullptr ? Error::NotFound :
        SetCheckbox(name, !cbItem->Value());
}

Config::Error Config::SetSpin(const std::string &name, int value)
{
    Item *item = ItemAt(name);

    if (item == nullptr)
        return Error::NotFound;
        
    SpinItem *sItem = dynamic_cast<SpinItem *>(item);

    if (sItem == nullptr)
        return Error::WrongType;

    return sItem->SetValue(value);
}

Config::Error Config::SetSpinClamped(const std::string &name, int value)
{
    Item *item = ItemAt(name);

    if (item == nullptr)
        return Error::NotFound;
        
    SpinItem *sItem = dynamic_cast<SpinItem *>(item);

    if (sItem == nullptr)
        return Error::WrongType;

    sItem->SetValueClamped(value);
    return Error::None;
}

Config::Error Config::SetCombo(const std::string &name, const std::string &value)
{
    Item *item = ItemAt(name);

    if (item == nullptr)
        return Error::NotFound;
        
    ComboItem *cItem = dynamic_cast<ComboItem *>(item);

    if (cItem == nullptr)
        return Error::WrongType;

    return cItem->SetValue(value);
}

Config::Error Config::SetButton(const std::string &name)
{
    Item *item = ItemAt(name);

    if (item == nullptr)
        return Error::NotFound;
        
    ButtonItem *bItem = dynamic_cast<ButtonItem *>(item);

    if (bItem == nullptr)
        return Error::WrongType;

    bItem->SetValue();
    return Error::None;
}

Config::Error Config::SetString(const std::string &name, const std::string &value)
{
    Item *item = ItemAt(name);

    if (item == nullptr)
        return Error::NotFound;
        
    StringItem *sItem = dynamic_cast<StringItem *>(item);

    if (sItem == nullptr)
        return Error::WrongType;

    sItem->SetValue(value);
    return Error::None;
}

#if 0 // standalone test harness
#include <iostream>
void cbCallback(const Config::CheckboxItem &item, void *context)
{
    // const Config::CheckboxItem &cbItem =
    //    dynamic_cast<const Config::CheckboxItem &>(item);
    std::cout << "cbCallback: fire, context " << context << " value "
              << std::boolalpha << item.Value() << '\n';
}

int main()
{
    Config cfg;
    void *context = (void *) 0xdeadbeef;
    cfg.Register(Config::CheckboxItem("canResign", "can machine resign",
                                      false,
                                      std::bind(cbCallback,
                                                std::placeholders::_1,
                                                context)));
    cfg.SetCheckbox("canResign", true);
    return 0;
}
#endif // standalone test harness
