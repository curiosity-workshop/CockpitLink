#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    bool expect(
        bool condition,
        std::string_view message)
    {
        if (!condition)
        {
            std::cerr << message << '\n';
            return false;
        }

        return true;
    }

    std::optional<std::string> readFile(
        const std::filesystem::path& path)
    {
        std::ifstream input{ path };

        if (!input)
        {
            return std::nullopt;
        }

        std::ostringstream buffer;
        buffer << input.rdbuf();
        return buffer.str();
    }

    bool containsField(
        std::string_view text,
        std::string_view field)
    {
        const std::string needle =
            "\"" + std::string{ field } + "\"";
        return text.find(needle) != std::string_view::npos;
    }

    std::optional<std::size_t> findArrayStart(
        std::string_view text,
        std::string_view field)
    {
        const std::string needle =
            "\"" + std::string{ field } + "\"";
        const auto fieldOffset =
            text.find(needle);

        if (fieldOffset == std::string_view::npos)
        {
            return std::nullopt;
        }

        const auto colonOffset =
            text.find(':', fieldOffset + needle.size());

        if (colonOffset == std::string_view::npos)
        {
            return std::nullopt;
        }

        const auto bracketOffset =
            text.find('[', colonOffset + 1);

        if (bracketOffset == std::string_view::npos)
        {
            return std::nullopt;
        }

        return bracketOffset;
    }

    std::vector<std::string> extractArrayObjects(
        std::string_view text,
        std::string_view field)
    {
        std::vector<std::string> objects;
        const auto arrayStart =
            findArrayStart(text, field);

        if (!arrayStart)
        {
            return objects;
        }

        bool inString = false;
        bool escaped = false;
        int arrayDepth = 0;
        int objectDepth = 0;
        std::size_t objectStart = 0;

        for (std::size_t offset = *arrayStart;
            offset < text.size();
            ++offset)
        {
            const char current =
                text[offset];

            if (inString)
            {
                if (escaped)
                {
                    escaped = false;
                }
                else if (current == '\\')
                {
                    escaped = true;
                }
                else if (current == '"')
                {
                    inString = false;
                }

                continue;
            }

            if (current == '"')
            {
                inString = true;
                continue;
            }

            if (current == '[')
            {
                ++arrayDepth;
                continue;
            }

            if (current == ']')
            {
                --arrayDepth;

                if (arrayDepth == 0)
                {
                    break;
                }

                continue;
            }

            if (arrayDepth != 1)
            {
                continue;
            }

            if (current == '{')
            {
                if (objectDepth == 0)
                {
                    objectStart = offset;
                }

                ++objectDepth;
            }
            else if (current == '}')
            {
                --objectDepth;

                if (objectDepth == 0)
                {
                    objects.emplace_back(
                        text.substr(
                            objectStart,
                            offset - objectStart + 1));
                }
            }
        }

        return objects;
    }

    std::optional<std::string> stringField(
        std::string_view object,
        std::string_view field)
    {
        const std::regex expression{
            "\"" + std::string{ field } + "\"\\s*:\\s*\"([^\"]*)\""
        };
        std::cmatch match;

        if (!std::regex_search(
                object.data(),
                object.data() + object.size(),
                match,
                expression))
        {
            return std::nullopt;
        }

        return match[1].str();
    }

    bool isAllowed(
        const std::set<std::string>& allowed,
        const std::optional<std::string>& value)
    {
        return value &&
            allowed.find(*value) != allowed.end();
    }
}

int main(
    int argc,
    char** argv)
{
    if (argc != 2)
    {
        std::cerr
            << "usage: BehaviorCatalogValidationTests <catalog.json>\n";
        return 1;
    }

    const auto catalog =
        readFile(argv[1]);

    bool passed = true;

    passed &= expect(
        catalog.has_value(),
        "catalog file should be readable");

    if (!catalog)
    {
        return 1;
    }

    passed &= expect(
        containsField(*catalog, "catalogVersion"),
        "catalogVersion is required");
    passed &= expect(
        containsField(*catalog, "name"),
        "catalog name is required");
    passed &= expect(
        containsField(*catalog, "behaviors"),
        "behaviors array is required");

    const auto behaviors =
        extractArrayObjects(*catalog, "behaviors");

    passed &= expect(
        !behaviors.empty(),
        "catalog should contain at least one behavior");

    const std::regex behaviorIdPattern{
        "^[a-z][a-z0-9_]*(\\.[a-z0-9_]+)+$"
    };
    const std::set<std::string> allowedKinds{
        "toggle",
        "momentary",
        "axis",
        "display",
        "enum",
        "data"
    };
    const std::set<std::string> allowedValueTypes{
        "bool",
        "int",
        "float",
        "string",
        "data",
        "enum"
    };
    const std::set<std::string> allowedCapabilities{
        "native",
        "unsupported",
        "emulatedByCommand",
        "emulatedByReadWrite",
        "readOnly",
        "writeOnly"
    };

    std::set<std::string> behaviorIds;

    for (const auto& behavior : behaviors)
    {
        const auto id =
            stringField(behavior, "id");
        const std::string label =
            id ? *id : "<missing id>";

        passed &= expect(
            id.has_value(),
            "behavior id is required");
        passed &= expect(
            id && std::regex_match(*id, behaviorIdPattern),
            "behavior id must be lowercase dot-separated: " + label);
        passed &= expect(
            id && id->find('[') == std::string::npos &&
                id->find(']') == std::string::npos,
            "behavior id must not expose array syntax: " + label);
        passed &= expect(
            id && behaviorIds.insert(*id).second,
            "behavior id must be unique: " + label);

        passed &= expect(
            containsField(behavior, "label"),
            "behavior label is required: " + label);
        passed &= expect(
            isAllowed(allowedKinds, stringField(behavior, "kind")),
            "behavior kind is invalid: " + label);
        passed &= expect(
            isAllowed(allowedValueTypes, stringField(behavior, "valueType")),
            "behavior valueType is invalid: " + label);

        passed &= expect(
            containsField(behavior, "desiredCapability"),
            "desiredCapability is required: " + label);
        passed &= expect(
            containsField(behavior, "read") &&
                containsField(behavior, "write") &&
                containsField(behavior, "command"),
            "read/write/command direction keys are required: " + label);
        passed &= expect(
            containsField(behavior, "defaultUpdate"),
            "defaultUpdate is required: " + label);
        passed &= expect(
            containsField(behavior, "rateMs") &&
                containsField(behavior, "bucket"),
            "defaultUpdate rateMs and bucket are required: " + label);
        passed &= expect(
            containsField(behavior, "bindings"),
            "bindings are required: " + label);

        const std::regex capabilityPattern{
            "\"(read|write|command)\"\\s*:\\s*\"([^\"]+)\""
        };
        const auto begin =
            std::sregex_iterator{
                behavior.begin(),
                behavior.end(),
                capabilityPattern
            };
        const auto end =
            std::sregex_iterator{};

        for (auto iterator = begin;
            iterator != end;
            ++iterator)
        {
            const std::string capability =
                (*iterator)[2].str();

            passed &= expect(
                allowedCapabilities.find(capability) !=
                    allowedCapabilities.end(),
                "binding capability is invalid on " + label +
                    ": " + capability);
        }
    }

    return passed ? 0 : 1;
}
