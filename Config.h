//--------------------------------------------------------------------------
//                   Config.h - engine option-setting API
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

#ifndef CONFIG_H
#define CONFIG_H

#include <algorithm>
#include <assert.h>
#include <functional>
#include <map>
#include <string>
#include <vector>

// To support UCI and Winboard equivalents, we need to support at least:
// checkbox (true/false)
// spin (integer w/in a range)
// combo (string multiple choice)
// button (a simple trigger)
// string (a free-form string)

// not used yet
// #define kConfigHistoryWindowSpin "historyWindow"
// #define kConfigTranpositionTableSizeSpin "limits/transpositionTableSize"

class Config
{
public:
    // Pre-defined config items (see Config.cpp for a better decription of these
    //  variables:)
    static const char
        *const MaxDepthSpin, *const MaxDepthDescription,
        *const MaxNodesSpin, *const MaxNodesDescription,
        *const RandomMovesCheckbox, *const RandomMovesDescription,
        *const CanResignCheckbox, *const CanResignDescription;
    
    Config() = default;
    Config(const Config &other) = default;
    
#if 0
    enum class ItemType
    {
        Unknown, // No such item
        Checkbox, // boolean checkbox
        Spin,    // integer range
        Combo,   // string multiple-choice
        Button,  // button
        String   // free-form string
    };
#endif
    
    ~Config(); // dtor

    enum class Error
    {
        None,          // no error
        NotFound,      // item not found
        WrongType,     // tried to invoke method of wrong type for this item
        InvalidValue,  // value out of range, or not a valid choice
        AlreadyExists // Item already exists (Register() only)
    };

    const char *ErrorString(Error error) const;
    
    Error SetCheckbox(const std::string &name, bool value);
    Error SetSpin(const std::string &name, int value);
    Error SetSpinClamped(const std::string &name, int value);
    Error SetCombo(const std::string &name, const std::string &value);
    Error SetButton(const std::string &name);
    Error SetString(const std::string &name, const std::string &value);

    // Do a type_id check to figure this out.
    // ItemType ItemType(const std::string &name) const;
    class Item
    {
    protected:
        Item(const std::string &name, const std::string &description);
        Item(const Item &other) = default;
    public:
        virtual Item *Clone() const;
        virtual ~Item() = default;
        inline std::string Name() const;
        inline std::string Description() const;
    private:
        std::string name;
        std::string description;
    };

    Error Register(const Item &item);

    // Retrieve an item by index.  This allows item discovery.  Returns item at
    //  index 'idx' (or nullptr if no such item exists).
    const Item *ItemAt(int idx) const;
    
    // Retrieve an item by name.  Returns nullptr if no such item exists.
    Item *ItemAt(const std::string &name) const;
    
    class CheckboxItem final : public Item
    {
    public:
        using CheckboxChangedFunc = std::function<void(const CheckboxItem &)>;
        CheckboxItem(const std::string &name, const std::string &description,
                     bool defaultValue, CheckboxChangedFunc callback);
        CheckboxItem(const CheckboxItem &other) = default;
        CheckboxItem *Clone() const override;
        inline bool Value() const;
        void SetValue(bool value);
    private:
        bool value = 0;
        CheckboxChangedFunc callback;
    };

    const CheckboxItem *CheckboxItemAt(const std::string &name) const;
    
    class SpinItem final : public Item
    {
    public:
        using SpinChangedFunc = std::function<void(const SpinItem &)>;
        SpinItem(const std::string &name, const std::string &description,
                 int min, int defaultValue, int max, SpinChangedFunc callback);
        SpinItem(const SpinItem &other) = default;
        SpinItem *Clone() const override;
        inline int Value() const;
        inline int Min() const;
        inline int Max() const;
        Error SetValue(int value);
        void SetValueClamped(int value);
    private:
        int value = 0;
        int min = 0, max = 0; // these are meant to be inclusive
        SpinChangedFunc callback;
    };

    const SpinItem *SpinItemAt(const std::string &name) const;
    
    using ComboChoices = std::vector<std::string>;
    class ComboItem final : public Item
    {
    public:
        using ComboChangedFunc = std::function<void(const ComboItem &)>;
        ComboItem(const std::string &name, const std::string &description,
                  const std::string &defaultValue, const ComboChoices &choices,
                  ComboChangedFunc callback);
        ComboItem(const ComboItem &other) = default;
        ComboItem *Clone() const override;
        inline std::string Value() const;
        inline ComboChoices Choices() const;
        Error SetValue(const std::string &value);
    private:
        std::string value;
        ComboChoices choices;
        ComboChangedFunc callback;
    };

    class StringItem final : public Item
    {
    public:
        using StringChangedFunc = std::function<void(const StringItem &)>;
        StringItem(const std::string &name, const std::string &description,
                   const std::string &defaultValue, StringChangedFunc callback);
        StringItem(const StringItem &other) = default;
        StringItem *Clone() const override;
        inline std::string Value() const;
        void SetValue(const std::string &value);
    private:
        std::string value;
        StringChangedFunc callback;
    };

    class ButtonItem final : public Item
    {
    public:
        using ButtonChangedFunc = std::function<void(const ButtonItem &)>;
        ButtonItem(const std::string &name, const std::string &description,
                   ButtonChangedFunc callback);
        ButtonItem(const ButtonItem &other) = default;
        ButtonItem *Clone() const override;
        void SetValue(); // 'pushes' the button
    private:
        ButtonChangedFunc callback;
    };

private:
    std::map<std::string, Item *> itemMap;
};

inline Config::Item::Item(const std::string &name,
                          const std::string &description) :
              name(name), description(description) {}

inline Config::Item *Config::Item::Clone() const
{
    return new Item(*this);
}

inline std::string Config::Item::Name() const
{
    return name;
}

inline std::string Config::Item::Description() const
{
    return description;
}

inline Config::CheckboxItem::CheckboxItem(const std::string &name,
                                          const std::string &description,
                                          bool defaultValue,
                                          CheckboxChangedFunc callback) :
              Item(name, description), value(defaultValue), callback(callback)
{
}

inline bool Config::CheckboxItem::Value() const
{
    return value;
}

inline Config::CheckboxItem *Config::CheckboxItem::Clone() const
{
    return new CheckboxItem(*this);
}

inline Config::SpinItem::SpinItem(const std::string &name,
                                  const std::string &description,
                                  int min, int defaultValue, int max,
                                  SpinChangedFunc callback) :
              Item(name, description), value(defaultValue), min(min), max(max),
              callback(callback)
{
    assert(min <= max);
}

inline Config::SpinItem *Config::SpinItem::Clone() const
{
    return new SpinItem(*this);
}

inline int Config::SpinItem::Value() const
{
    return value;
}

inline int Config::SpinItem::Min() const
{
    return min;
}

inline int Config::SpinItem::Max() const
{
    return max;
}

inline const Config::SpinItem *Config::SpinItemAt(const std::string &name) const
{
    return dynamic_cast<const SpinItem *>(ItemAt(name));
}

inline const Config::CheckboxItem *Config::CheckboxItemAt(const std::string &name) const
{
    return dynamic_cast<const CheckboxItem *>(ItemAt(name));
}

inline Config::ComboItem::ComboItem(const std::string &name,
                                    const std::string &description,
                                    const std::string &defaultValue,
                                    const ComboChoices &choices,
                                    ComboChangedFunc callback) :
              Item(name, description), value(defaultValue), choices(choices),
              callback(callback)
{
    assert(std::find(choices.begin(), choices.end(), value) != choices.end());
}

inline Config::ComboItem *Config::ComboItem::Clone() const
{
    return new ComboItem(*this);
}

inline std::string Config::ComboItem::Value() const
{
    return value;
}

inline Config::ComboChoices Config::ComboItem::Choices() const
{
    return choices;
}

inline Config::StringItem::StringItem(const std::string &name,
                                      const std::string &description,
                                      const std::string &defaultValue,
                                      StringChangedFunc callback) :
              Item(name, description), value(defaultValue), callback(callback)
{
}

inline Config::StringItem *Config::StringItem::Clone() const
{
    return new StringItem(*this);
}

inline std::string Config::StringItem::Value() const
{
    return value;
}

inline Config::ButtonItem::ButtonItem(const std::string &name,
                                      const std::string &description,
                                      ButtonChangedFunc callback) :
              Item(name, description), callback(callback) {}

inline Config::ButtonItem *Config::ButtonItem::Clone() const
{
    return new ButtonItem(*this);
}

#endif // CONFIG_H
