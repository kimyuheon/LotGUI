#pragma once

#include "core/widget_tree.h"
#include "text/text_layout.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace lotui::declarative {

class LotmlError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

struct LotmlElement {
    std::string type;
    std::string text;
    std::unordered_map<std::string, std::string> attributes;
    std::vector<LotmlElement> children;
    int line{0};

    const std::string* findAttribute(std::string_view name) const noexcept;
};

enum class PropertyType {
    String,
    Number,
    Boolean,
    Color,
    Insets,
    Enumeration,
    Event,
};

struct PropertyDescriptor {
    std::string name;
    PropertyType type{PropertyType::String};
    std::string defaultValue;
    bool required{false};
};

struct WidgetDescriptor {
    std::string type;
    bool allowsChildren{false};
    std::vector<PropertyDescriptor> properties;
};

class BuildContext;
using WidgetFactory = std::function<std::unique_ptr<Widget>(
    const LotmlElement& element,
    BuildContext& context)>;

class WidgetRegistry {
public:
    void registerWidget(
        WidgetDescriptor descriptor,
        WidgetFactory factory);
    const WidgetDescriptor* find(std::string_view type) const noexcept;
    std::vector<WidgetDescriptor> descriptors() const;

private:
    friend class BuildContext;

    struct Entry {
        WidgetDescriptor descriptor;
        WidgetFactory factory;
    };
    std::unordered_map<std::string, Entry> entries_;
};

struct LoadOptions {
    std::shared_ptr<const TextEngine> textEngine;
    std::unordered_map<std::string, std::function<void()>> events;
};

class BuildContext {
public:
    std::unique_ptr<Widget> buildChild(const LotmlElement& element);
    std::shared_ptr<const TextEngine> textEngine() const noexcept;
    std::function<void()> event(std::string_view name) const;

private:
    friend class LotmlLoader;

    struct State;
    explicit BuildContext(State& state) noexcept;
    State* state_{nullptr};
};

class LoadedUi {
public:
    LoadedUi(LoadedUi&&) noexcept = default;
    LoadedUi& operator=(LoadedUi&&) noexcept = default;
    LoadedUi(const LoadedUi&) = delete;
    LoadedUi& operator=(const LoadedUi&) = delete;

    WidgetTree& tree() noexcept;
    const WidgetTree& tree() const noexcept;
    Widget* find(std::string_view id) noexcept;
    const Widget* find(std::string_view id) const noexcept;

private:
    friend class LotmlLoader;
    LoadedUi(
        std::unique_ptr<WidgetTree> tree,
        std::unordered_map<std::string, Widget*> widgetsById);

    std::unique_ptr<WidgetTree> tree_;
    std::unordered_map<std::string, Widget*> widgetsById_;
};

class LotmlLoader {
public:
    LotmlLoader();
    explicit LotmlLoader(WidgetRegistry registry);

    WidgetRegistry& registry() noexcept;
    const WidgetRegistry& registry() const noexcept;

    LoadedUi loadString(
        std::string_view xml,
        LoadOptions options = {}) const;
    LoadedUi loadFile(
        const std::filesystem::path& file,
        LoadOptions options = {}) const;

private:
    WidgetRegistry registry_;
};

WidgetRegistry createDefaultWidgetRegistry();

} // namespace lotui::declarative
