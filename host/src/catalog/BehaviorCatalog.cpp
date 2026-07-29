#include <cockpitlink/catalog/BehaviorCatalog.h>

#include <algorithm>
#include <charconv>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <map>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <variant>

namespace
{
    struct JsonValue;
    using JsonObject = std::map<std::string, JsonValue, std::less<>>;
    using JsonArray = std::vector<JsonValue>;

    struct JsonValue
    {
        using Storage = std::variant<
            std::nullptr_t,
            bool,
            double,
            std::string,
            JsonArray,
            JsonObject>;

        Storage storage;

        const JsonObject* object() const
        {
            return std::get_if<JsonObject>(&storage);
        }

        const JsonArray* array() const
        {
            return std::get_if<JsonArray>(&storage);
        }

        const std::string* string() const
        {
            return std::get_if<std::string>(&storage);
        }

        const double* number() const
        {
            return std::get_if<double>(&storage);
        }

        const bool* boolean() const
        {
            return std::get_if<bool>(&storage);
        }
    };

    class JsonParser
    {
    public:
        explicit JsonParser(
            std::string_view text)
            : text_{ text }
        {
        }

        JsonValue parse()
        {
            skipWhitespace();
            auto value = parseValue();
            skipWhitespace();

            if (offset_ != text_.size())
            {
                fail("unexpected trailing content");
            }

            return value;
        }

    private:
        JsonValue parseValue()
        {
            if (offset_ >= text_.size())
            {
                fail("unexpected end of input");
            }

            switch (text_[offset_])
            {
            case '{':
                return JsonValue{ parseObject() };
            case '[':
                return JsonValue{ parseArray() };
            case '"':
                return JsonValue{ parseString() };
            case 't':
                consumeLiteral("true");
                return JsonValue{ true };
            case 'f':
                consumeLiteral("false");
                return JsonValue{ false };
            case 'n':
                consumeLiteral("null");
                return JsonValue{ nullptr };
            default:
                if (text_[offset_] == '-' ||
                    std::isdigit(
                        static_cast<unsigned char>(text_[offset_])))
                {
                    return JsonValue{ parseNumber() };
                }

                fail("unexpected token");
            }
        }

        JsonObject parseObject()
        {
            JsonObject object;
            consume('{');
            skipWhitespace();

            if (tryConsume('}'))
            {
                return object;
            }

            while (true)
            {
                if (offset_ >= text_.size() ||
                    text_[offset_] != '"')
                {
                    fail("object key must be a string");
                }

                auto key = parseString();
                skipWhitespace();
                consume(':');
                skipWhitespace();

                if (!object.emplace(
                        std::move(key),
                        parseValue()).second)
                {
                    fail("duplicate object key");
                }

                skipWhitespace();

                if (tryConsume('}'))
                {
                    break;
                }

                consume(',');
                skipWhitespace();
            }

            return object;
        }

        JsonArray parseArray()
        {
            JsonArray array;
            consume('[');
            skipWhitespace();

            if (tryConsume(']'))
            {
                return array;
            }

            while (true)
            {
                array.push_back(parseValue());
                skipWhitespace();

                if (tryConsume(']'))
                {
                    break;
                }

                consume(',');
                skipWhitespace();
            }

            return array;
        }

        std::string parseString()
        {
            std::string value;
            consume('"');

            while (offset_ < text_.size())
            {
                const char current = text_[offset_++];

                if (current == '"')
                {
                    return value;
                }

                if (static_cast<unsigned char>(current) < 0x20)
                {
                    fail("control character in string");
                }

                if (current != '\\')
                {
                    value.push_back(current);
                    continue;
                }

                if (offset_ >= text_.size())
                {
                    fail("incomplete string escape");
                }

                const char escaped = text_[offset_++];

                switch (escaped)
                {
                case '"':
                case '\\':
                case '/':
                    value.push_back(escaped);
                    break;
                case 'b':
                    value.push_back('\b');
                    break;
                case 'f':
                    value.push_back('\f');
                    break;
                case 'n':
                    value.push_back('\n');
                    break;
                case 'r':
                    value.push_back('\r');
                    break;
                case 't':
                    value.push_back('\t');
                    break;
                case 'u':
                    appendUnicodeEscape(value);
                    break;
                default:
                    fail("invalid string escape");
                }
            }

            fail("unterminated string");
        }

        void appendUnicodeEscape(
            std::string& value)
        {
            if (offset_ + 4 > text_.size())
            {
                fail("incomplete unicode escape");
            }

            unsigned codePoint = 0;

            for (int index = 0; index < 4; ++index)
            {
                const char digit = text_[offset_++];
                codePoint <<= 4;

                if (digit >= '0' && digit <= '9')
                {
                    codePoint += static_cast<unsigned>(digit - '0');
                }
                else if (digit >= 'a' && digit <= 'f')
                {
                    codePoint += static_cast<unsigned>(digit - 'a' + 10);
                }
                else if (digit >= 'A' && digit <= 'F')
                {
                    codePoint += static_cast<unsigned>(digit - 'A' + 10);
                }
                else
                {
                    fail("invalid unicode escape");
                }
            }

            if (codePoint <= 0x7f)
            {
                value.push_back(static_cast<char>(codePoint));
            }
            else if (codePoint <= 0x7ff)
            {
                value.push_back(
                    static_cast<char>(0xc0 | (codePoint >> 6)));
                value.push_back(
                    static_cast<char>(0x80 | (codePoint & 0x3f)));
            }
            else
            {
                value.push_back(
                    static_cast<char>(0xe0 | (codePoint >> 12)));
                value.push_back(
                    static_cast<char>(
                        0x80 | ((codePoint >> 6) & 0x3f)));
                value.push_back(
                    static_cast<char>(0x80 | (codePoint & 0x3f)));
            }
        }

        double parseNumber()
        {
            const std::size_t start = offset_;

            if (text_[offset_] == '-')
            {
                ++offset_;
            }

            if (offset_ >= text_.size())
            {
                fail("incomplete number");
            }

            if (text_[offset_] == '0')
            {
                ++offset_;
            }
            else
            {
                requireDigits();
            }

            if (offset_ < text_.size() &&
                text_[offset_] == '.')
            {
                ++offset_;
                requireDigits();
            }

            if (offset_ < text_.size() &&
                (text_[offset_] == 'e' ||
                    text_[offset_] == 'E'))
            {
                ++offset_;

                if (offset_ < text_.size() &&
                    (text_[offset_] == '+' ||
                        text_[offset_] == '-'))
                {
                    ++offset_;
                }

                requireDigits();
            }

            const std::string number{
                text_.substr(start, offset_ - start)
            };
            char* end = nullptr;
            const double result =
                std::strtod(number.c_str(), &end);

            if (end != number.c_str() + number.size())
            {
                fail("invalid number");
            }

            return result;
        }

        void requireDigits()
        {
            const std::size_t start = offset_;

            while (offset_ < text_.size() &&
                std::isdigit(
                    static_cast<unsigned char>(text_[offset_])))
            {
                ++offset_;
            }

            if (start == offset_)
            {
                fail("expected digit");
            }
        }

        void consumeLiteral(
            std::string_view literal)
        {
            if (text_.substr(offset_, literal.size()) != literal)
            {
                fail("invalid literal");
            }

            offset_ += literal.size();
        }

        void consume(
            char expected)
        {
            if (offset_ >= text_.size() ||
                text_[offset_] != expected)
            {
                fail(std::string{ "expected '" } + expected + "'");
            }

            ++offset_;
        }

        bool tryConsume(
            char expected)
        {
            if (offset_ < text_.size() &&
                text_[offset_] == expected)
            {
                ++offset_;
                return true;
            }

            return false;
        }

        void skipWhitespace()
        {
            while (offset_ < text_.size() &&
                std::isspace(
                    static_cast<unsigned char>(text_[offset_])))
            {
                ++offset_;
            }
        }

        [[noreturn]] void fail(
            const std::string& message) const
        {
            throw std::runtime_error{
                message + " at byte " + std::to_string(offset_)
            };
        }

        std::string_view text_;
        std::size_t offset_ = 0;
    };

    const JsonValue* field(
        const JsonObject& object,
        std::string_view name)
    {
        const auto found = object.find(name);
        return found == object.end() ? nullptr : &found->second;
    }

    const JsonObject* objectField(
        const JsonObject& object,
        std::string_view name)
    {
        const auto* value = field(object, name);
        return value ? value->object() : nullptr;
    }

    std::optional<std::string> stringField(
        const JsonObject& object,
        std::string_view name)
    {
        const auto* value = field(object, name);
        const auto* string = value ? value->string() : nullptr;
        return string ? std::optional<std::string>{ *string } : std::nullopt;
    }

    std::optional<double> numberField(
        const JsonObject& object,
        std::string_view name)
    {
        const auto* value = field(object, name);
        const auto* number = value ? value->number() : nullptr;
        return number ? std::optional<double>{ *number } : std::nullopt;
    }

    std::optional<bool> boolField(
        const JsonObject& object,
        std::string_view name)
    {
        const auto* value = field(object, name);
        const auto* boolean = value ? value->boolean() : nullptr;
        return boolean ? std::optional<bool>{ *boolean } : std::nullopt;
    }

    template<typename Enum>
    std::optional<Enum> parseEnum(
        const std::optional<std::string>& value,
        std::initializer_list<std::pair<std::string_view, Enum>> entries)
    {
        if (!value)
        {
            return std::nullopt;
        }

        for (const auto& [name, result] : entries)
        {
            if (*value == name)
            {
                return result;
            }
        }

        return std::nullopt;
    }

    std::optional<cockpitlink::catalog::BehaviorKind> behaviorKind(
        const std::optional<std::string>& value)
    {
        using enum cockpitlink::catalog::BehaviorKind;
        return parseEnum<cockpitlink::catalog::BehaviorKind>(value, {
            { "toggle", Toggle },
            { "momentary", Momentary },
            { "axis", Axis },
            { "display", Display },
            { "enum", Enum },
            { "data", Data }
        });
    }

    std::optional<cockpitlink::catalog::ValueType> valueType(
        const std::optional<std::string>& value)
    {
        using enum cockpitlink::catalog::ValueType;
        return parseEnum<cockpitlink::catalog::ValueType>(value, {
            { "bool", Boolean },
            { "int", Int },
            { "float", Float },
            { "string", String },
            { "data", Data },
            { "enum", Enum }
        });
    }

    std::optional<cockpitlink::catalog::Capability> capability(
        const JsonObject& object,
        std::string_view name)
    {
        using enum cockpitlink::catalog::Capability;
        return parseEnum<cockpitlink::catalog::Capability>(
            stringField(object, name), {
            { "native", Native },
            { "unsupported", Unsupported },
            { "emulatedByCommand", EmulatedByCommand },
            { "emulatedByReadWrite", EmulatedByReadWrite },
            { "readOnly", ReadOnly },
            { "writeOnly", WriteOnly }
        });
    }

    std::optional<cockpitlink::catalog::WriteStrategy> writeStrategy(
        const std::optional<std::string>& value)
    {
        using enum cockpitlink::catalog::WriteStrategy;
        return parseEnum<cockpitlink::catalog::WriteStrategy>(value, {
            { "directSet", DirectSet },
            { "setViaToggleWhenKnown", SetViaToggleWhenKnown },
            { "commandOnOff", CommandOnOff },
            { "momentaryCommand", MomentaryCommand },
            { "unsupported", Unsupported }
        });
    }

    std::optional<cockpitlink::catalog::Scale> parseScale(
        const JsonObject& object,
        std::vector<std::string>& errors,
        const std::string& context)
    {
        const auto fromMin = numberField(object, "fromMin");
        const auto fromMax = numberField(object, "fromMax");
        const auto toMin = numberField(object, "toMin");
        const auto toMax = numberField(object, "toMax");

        if (!fromMin || !fromMax || !toMin || !toMax)
        {
            errors.push_back(context + " scale requires fromMin, fromMax, toMin, and toMax");
            return std::nullopt;
        }

        if (*fromMin == *fromMax)
        {
            errors.push_back(context + " scale input range must not be empty");
            return std::nullopt;
        }

        return cockpitlink::catalog::Scale{
            *fromMin,
            *fromMax,
            *toMin,
            *toMax
        };
    }

    std::optional<cockpitlink::catalog::DataRefOperation>
    parseDataRefOperation(
        const JsonObject& object,
        std::vector<std::string>& errors,
        const std::string& context)
    {
        const auto dataRef = stringField(object, "dataref");
        const auto type = stringField(object, "type");

        if (!dataRef || dataRef->empty())
        {
            errors.push_back(context + " requires a dataref");
        }

        if (!type || type->empty())
        {
            errors.push_back(context + " requires a type");
        }

        if (!dataRef || dataRef->empty() || !type || type->empty())
        {
            return std::nullopt;
        }

        cockpitlink::catalog::DataRefOperation result;
        result.dataRef = *dataRef;
        result.type = *type;

        if (const auto index = numberField(object, "index"))
        {
            result.index = static_cast<int>(*index);
        }

        if (const auto* indicesValue = field(object, "indices"))
        {
            const auto* indices = indicesValue->array();

            if (!indices)
            {
                errors.push_back(context + " indices must be an array");
            }
            else
            {
                for (const auto& item : *indices)
                {
                    if (const auto* number = item.number())
                    {
                        result.indices.push_back(
                            static_cast<int>(*number));
                    }
                    else
                    {
                        errors.push_back(
                            context + " indices must contain numbers");
                    }
                }
            }
        }

        if (const auto* scale = objectField(object, "scale"))
        {
            result.scale = parseScale(*scale, errors, context);
        }

        return result;
    }

    std::optional<cockpitlink::catalog::XPlaneBinding>
    parseXPlaneBinding(
        const JsonObject& object,
        std::vector<std::string>& errors,
        const std::string& behaviorId)
    {
        const auto* defaultBinding = objectField(object, "default");

        if (!defaultBinding)
        {
            errors.push_back(
                behaviorId + " xplane binding requires default");
            return std::nullopt;
        }

        const auto* capabilities =
            objectField(*defaultBinding, "capability");

        if (!capabilities)
        {
            errors.push_back(
                behaviorId + " xplane binding requires capability");
            return std::nullopt;
        }

        const auto readCapability = capability(*capabilities, "read");
        const auto writeCapability = capability(*capabilities, "write");
        const auto commandCapability = capability(*capabilities, "command");

        if (!readCapability || !writeCapability || !commandCapability)
        {
            errors.push_back(
                behaviorId + " xplane capability values are invalid");
            return std::nullopt;
        }

        cockpitlink::catalog::XPlaneBinding result;
        result.capability = {
            *readCapability,
            *writeCapability,
            *commandCapability
        };

        if (const auto* read = objectField(*defaultBinding, "read"))
        {
            result.read = parseDataRefOperation(
                *read,
                errors,
                behaviorId + " xplane read");
        }

        if (const auto* write = objectField(*defaultBinding, "write"))
        {
            result.writeStrategy =
                writeStrategy(stringField(*write, "strategy"));
            result.toggleCommand =
                stringField(*write, "toggleCommand");
            result.requiresRead =
                boolField(*write, "requiresRead").value_or(false);

            if (!result.writeStrategy)
            {
                errors.push_back(
                    behaviorId + " xplane write strategy is invalid");
            }

            if (field(*write, "dataref"))
            {
                result.write = parseDataRefOperation(
                    *write,
                    errors,
                    behaviorId + " xplane write");
            }
            else if (result.writeStrategy ==
                cockpitlink::catalog::WriteStrategy::DirectSet)
            {
                errors.push_back(
                    behaviorId + " directSet write requires a dataref");
            }
        }

        return result;
    }
}

namespace cockpitlink::catalog
{
    std::uint32_t BehaviorCatalog::version() const
    {
        return version_;
    }

    const std::string& BehaviorCatalog::name() const
    {
        return name_;
    }

    const std::vector<Behavior>& BehaviorCatalog::behaviors() const
    {
        return behaviors_;
    }

    const Behavior* BehaviorCatalog::find(
        std::string_view behaviorId) const
    {
        const auto found = std::find_if(
            behaviors_.begin(),
            behaviors_.end(),
            [behaviorId](const Behavior& behavior)
            {
                return behavior.id == behaviorId;
            });

        return found == behaviors_.end() ? nullptr : &*found;
    }

    const Behavior* BehaviorCatalog::atHandle(
        std::uint16_t handle) const
    {
        return handle < behaviors_.size() ?
            &behaviors_[handle] :
            nullptr;
    }

    std::optional<std::uint16_t> BehaviorCatalog::handleFor(
        std::string_view behaviorId) const
    {
        const auto* behavior = find(behaviorId);

        if (!behavior)
        {
            return std::nullopt;
        }

        const auto offset = static_cast<std::size_t>(
            behavior - behaviors_.data());

        if (offset >= std::numeric_limits<std::uint16_t>::max())
        {
            return std::nullopt;
        }

        return static_cast<std::uint16_t>(offset);
    }

    std::optional<BehaviorCatalog> loadBehaviorCatalog(
        const std::filesystem::path& path,
        std::vector<std::string>& errors)
    {
        errors.clear();
        std::ifstream input{ path, std::ios::binary };

        if (!input)
        {
            errors.push_back(
                "cannot open catalog: " + path.string());
            return std::nullopt;
        }

        std::ostringstream buffer;
        buffer << input.rdbuf();

        JsonValue root;

        try
        {
            root = JsonParser{ buffer.str() }.parse();
        }
        catch (const std::exception& exception)
        {
            errors.push_back(
                std::string{ "invalid JSON: " } + exception.what());
            return std::nullopt;
        }

        const auto* rootObject = root.object();

        if (!rootObject)
        {
            errors.push_back("catalog root must be an object");
            return std::nullopt;
        }

        BehaviorCatalog catalog;
        const auto version = numberField(*rootObject, "catalogVersion");
        const auto name = stringField(*rootObject, "name");
        const auto* behaviorsValue = field(*rootObject, "behaviors");
        const auto* behaviors =
            behaviorsValue ? behaviorsValue->array() : nullptr;

        if (!version || *version < 1 ||
            *version != static_cast<std::uint32_t>(*version))
        {
            errors.push_back(
                "catalogVersion must be a positive integer");
        }
        else
        {
            catalog.version_ = static_cast<std::uint32_t>(*version);
        }

        if (!name || name->empty())
        {
            errors.push_back("catalog name is required");
        }
        else
        {
            catalog.name_ = *name;
        }

        if (!behaviors || behaviors->empty())
        {
            errors.push_back(
                "behaviors must be a non-empty array");
            return std::nullopt;
        }

        const std::regex idPattern{
            "^[a-z][a-z0-9_]*(\\.[a-z0-9_]+)+$"
        };

        for (std::size_t index = 0;
            index < behaviors->size();
            ++index)
        {
            const auto* object = (*behaviors)[index].object();
            const std::string context =
                "behavior[" + std::to_string(index) + "]";

            if (!object)
            {
                errors.push_back(context + " must be an object");
                continue;
            }

            Behavior behavior;
            const auto id = stringField(*object, "id");
            const auto label = stringField(*object, "label");
            const auto kind = behaviorKind(stringField(*object, "kind"));
            const auto type = valueType(stringField(*object, "valueType"));

            if (!id || !std::regex_match(*id, idPattern))
            {
                errors.push_back(
                    context + " id must be lowercase and dot-separated");
                continue;
            }

            if (catalog.find(*id))
            {
                errors.push_back("duplicate behavior id: " + *id);
                continue;
            }

            behavior.id = *id;

            if (!label || label->empty())
            {
                errors.push_back(behavior.id + " label is required");
            }
            else
            {
                behavior.label = *label;
            }

            if (!kind)
            {
                errors.push_back(behavior.id + " kind is invalid");
            }
            else
            {
                behavior.kind = *kind;
            }

            if (!type)
            {
                errors.push_back(behavior.id + " valueType is invalid");
            }
            else
            {
                behavior.valueType = *type;
            }

            const auto* desired =
                objectField(*object, "desiredCapability");

            if (!desired ||
                !boolField(*desired, "read") ||
                !boolField(*desired, "write") ||
                !boolField(*desired, "command"))
            {
                errors.push_back(
                    behavior.id +
                    " desiredCapability requires boolean read/write/command");
            }
            else
            {
                behavior.desiredRead =
                    *boolField(*desired, "read");
                behavior.desiredWrite =
                    *boolField(*desired, "write");
                behavior.desiredCommand =
                    *boolField(*desired, "command");
            }

            const auto* update =
                objectField(*object, "defaultUpdate");
            const auto rate = update ?
                numberField(*update, "rateMs") :
                std::nullopt;
            const auto bucket = update ?
                numberField(*update, "bucket") :
                std::nullopt;

            if (!rate || *rate < 0 ||
                *rate > std::numeric_limits<std::uint16_t>::max() ||
                !bucket || *bucket < 0 ||
                *bucket > std::numeric_limits<std::uint16_t>::max())
            {
                errors.push_back(
                    behavior.id +
                    " defaultUpdate requires uint16 rateMs and bucket");
            }
            else
            {
                behavior.updateRateMs =
                    static_cast<std::uint16_t>(*rate);
                behavior.updateBucket =
                    static_cast<std::uint16_t>(*bucket);
            }

            if (const auto* range = objectField(*object, "range"))
            {
                behavior.rangeMinimum = numberField(*range, "min");
                behavior.rangeMaximum = numberField(*range, "max");

                if (!behavior.rangeMinimum ||
                    !behavior.rangeMaximum ||
                    *behavior.rangeMinimum >= *behavior.rangeMaximum)
                {
                    errors.push_back(
                        behavior.id + " range is invalid");
                }
            }

            const auto* bindings =
                objectField(*object, "bindings");

            if (!bindings)
            {
                errors.push_back(behavior.id + " bindings are required");
            }
            else if (const auto* xplane =
                objectField(*bindings, "xplane"))
            {
                behavior.xplane =
                    parseXPlaneBinding(*xplane, errors, behavior.id);
            }

            catalog.behaviors_.push_back(std::move(behavior));
        }

        if (catalog.behaviors_.size() >=
            std::numeric_limits<std::uint16_t>::max())
        {
            errors.push_back(
                "catalog has too many behaviors for protocol handles");
        }

        return errors.empty() ?
            std::optional<BehaviorCatalog>{ std::move(catalog) } :
            std::nullopt;
    }
}
