/*
 * Copyright (c) 2021, Sam Atkins <atkinssj@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibWeb/CSS/MediaQuery.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/DOM/Window.h>

namespace Web::CSS {

NonnullRefPtr<MediaQuery> MediaQuery::create_not_all()
{
    auto media_query = new MediaQuery;
    media_query->m_negated = true;
    media_query->m_media_type = MediaType::All;

    return adopt_ref(*media_query);
}

String MediaQuery::MediaFeature::to_string() const
{
    switch (type) {
    case Type::IsTrue:
        return name;
    case Type::ExactValue:
        return String::formatted("{}:{}", name, value->to_string());
    case Type::MinValue:
        return String::formatted("min-{}:{}", name, value->to_string());
    case Type::MaxValue:
        return String::formatted("max-{}:{}", name, value->to_string());
    }

    VERIFY_NOT_REACHED();
}

bool MediaQuery::MediaFeature::evaluate(DOM::Window const& window) const
{
    auto queried_value = window.query_media_feature(name);
    if (!queried_value)
        return false;

    switch (type) {
    case Type::IsTrue:
        if (queried_value->has_number())
            return queried_value->to_number() != 0;
        if (queried_value->has_length())
            return queried_value->to_length().raw_value() != 0;
        if (queried_value->has_identifier())
            return queried_value->to_identifier() != ValueID::None;
        return false;

    case Type::ExactValue:
        return queried_value->equals(*value);

    case Type::MinValue:
        if (queried_value->has_number() && value->has_number())
            return queried_value->to_number() >= value->to_number();
        if (queried_value->has_length() && value->has_length()) {
            auto queried_length = queried_value->to_length();
            auto value_length = value->to_length();
            // FIXME: We should be checking that lengths are valid during parsing
            if (!value_length.is_absolute()) {
                dbgln("Media feature was given a non-absolute length, which is invalid! {}", value_length.to_string());
                return false;
            }
            return queried_length.absolute_length_to_px() >= value_length.absolute_length_to_px();
        }
        return false;

    case Type::MaxValue:
        if (queried_value->has_number() && value->has_number())
            return queried_value->to_number() <= value->to_number();
        if (queried_value->has_length() && value->has_length()) {
            auto queried_length = queried_value->to_length();
            auto value_length = value->to_length();
            // FIXME: We should be checking that lengths are valid during parsing
            if (!value_length.is_absolute()) {
                dbgln("Media feature was given a non-absolute length, which is invalid! {}", value_length.to_string());
                return false;
            }
            return queried_length.absolute_length_to_px() <= value_length.absolute_length_to_px();
        }
        return false;
    }

    VERIFY_NOT_REACHED();
}

String MediaQuery::MediaCondition::to_string() const
{
    StringBuilder builder;
    builder.append('(');
    switch (type) {
    case Type::Single:
        builder.append(feature.to_string());
        break;
    case Type::Not:
        builder.append("not ");
        builder.append(conditions.first().to_string());
        break;
    case Type::And:
        builder.join(" and ", conditions);
        break;
    case Type::Or:
        builder.join(" or ", conditions);
        break;
    }
    builder.append(')');
    return builder.to_string();
}

bool MediaQuery::MediaCondition::evaluate(DOM::Window const& window) const
{
    switch (type) {
    case Type::Single:
        return feature.evaluate(window);

    case Type::Not:
        return !conditions.first().evaluate(window);

    case Type::And:
        for (auto& child : conditions) {
            if (!child.evaluate(window))
                return false;
        }
        return true;

    case Type::Or:
        for (auto& child : conditions) {
            if (child.evaluate(window))
                return true;
        }
        return false;
    }

    VERIFY_NOT_REACHED();
}

String MediaQuery::to_string() const
{
    StringBuilder builder;

    if (m_negated)
        builder.append("not ");

    if (m_negated || m_media_type != MediaType::All || !m_media_condition) {
        switch (m_media_type) {
        case MediaType::All:
            builder.append("all");
            break;
        case MediaType::Aural:
            builder.append("aural");
            break;
        case MediaType::Braille:
            builder.append("braille");
            break;
        case MediaType::Embossed:
            builder.append("embossed");
            break;
        case MediaType::Handheld:
            builder.append("handheld");
            break;
        case MediaType::Print:
            builder.append("print");
            break;
        case MediaType::Projection:
            builder.append("projection");
            break;
        case MediaType::Screen:
            builder.append("screen");
            break;
        case MediaType::Speech:
            builder.append("speech");
            break;
        case MediaType::TTY:
            builder.append("tty");
            break;
        case MediaType::TV:
            builder.append("tv");
            break;
        }
        if (m_media_condition)
            builder.append(" and ");
    }

    if (m_media_condition) {
        builder.append(m_media_condition->to_string());
    }

    return builder.to_string();
}

bool MediaQuery::evaluate(DOM::Window const& window)
{
    auto matches_media = [](MediaType media) -> bool {
        switch (media) {
        case MediaType::All:
            return true;
        case MediaType::Print:
            // FIXME: Enable for printing, when we have printing!
            return false;
        case MediaType::Screen:
            // FIXME: Disable for printing, when we have printing!
            return true;
        // Deprecated, must never match:
        case MediaType::TTY:
        case MediaType::TV:
        case MediaType::Projection:
        case MediaType::Handheld:
        case MediaType::Braille:
        case MediaType::Embossed:
        case MediaType::Aural:
        case MediaType::Speech:
            return false;
        }
        VERIFY_NOT_REACHED();
    };

    bool result = matches_media(m_media_type);

    if (result && m_media_condition)
        result = m_media_condition->evaluate(window);

    m_matches = m_negated ? !result : result;
    return m_matches;
}

}
