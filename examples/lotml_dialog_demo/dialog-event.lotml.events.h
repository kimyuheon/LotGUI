#pragma once

#include "core/color.h"
#include "declarative/lotml.h"
#include "widgets/button.h"
#include "widgets/dialog.h"
#include "widgets/label.h"
#include "widgets/linear_layout.h"

namespace lotui::designer::layout_dialog_event {

inline void handle_showDialog_89224e6a22(
    DialogHost& host, const std::shared_ptr<const TextEngine>& textEngine) {
    TextStyle text;
    text.fontFamilies = {"Noto Sans KR"};
    text.fontSize = 15.0F;

    auto content = std::make_unique<Column>();
    LinearLayoutOptions contentLayout;
    contentLayout.spacing = 12.0F;
    contentLayout.crossAxisAlignment = CrossAxisAlignment::Stretch;
    content->setOptions(contentLayout);
    auto message = std::make_unique<Label>(
        textEngine, "버튼의 C++ 이벤트가 실행되었습니다.", text);
    message->setColor(colorFromSrgb(0xD1D9E4));
    content->addChild(std::move(message));

    auto actions = std::make_unique<Row>();
    LinearLayoutOptions actionLayout;
    actionLayout.mainAxisAlignment = MainAxisAlignment::End;
    actions->setOptions(actionLayout);
    auto caption = std::make_unique<Label>(textEngine, "확인", text);
    caption->setHorizontalAlignment(HorizontalTextAlignment::Center);
    caption->setVerticalAlignment(VerticalTextAlignment::Center);
    ButtonStyle buttonStyle;
    buttonStyle.normal = colorFromSrgb(0x31475F);
    buttonStyle.hovered = colorFromSrgb(0x3B5573);
    buttonStyle.pressed = colorFromSrgb(0x293749);
    buttonStyle.focusRing = colorFromSrgb(0x8EB8E5);
    actions->addChild(std::make_unique<Button>(std::move(caption),
        Size{84.0F, 34.0F}, [&host] { host.acceptModal(); }, buttonStyle));
    content->addChild(std::move(actions));

    DialogStyle dialogStyle;
    dialogStyle.background = colorFromSrgb(0x23272E);
    dialogStyle.titleDivider = colorFromSrgb(0x3B414A);
    dialogStyle.cornerRadius = 8.0F;
    dialogStyle.contentPadding = {24.0F, 16.0F, 24.0F, 16.0F};
    auto dialog = std::make_unique<Dialog>(
        std::move(content), Size{365.0F, 170.0F}, dialogStyle);
    text.fontSize = 18.0F;
    text.weight = FontWeight::SemiBold;
    dialog->setTitle(std::make_unique<Label>(textEngine, "이벤트 대화상자", text));
    host.showModal(std::move(dialog));
}

// LotUI handler insertion point

inline void bindEvents(declarative::LoadOptions& options, DialogHost*& host) {
    options.events.try_emplace("showDialog", [&host, text = options.textEngine] {
        if (host) handle_showDialog_89224e6a22(*host, text);
    });
    // LotUI binding insertion point
}

} // namespace lotui::designer::layout_dialog_event
