#pragma once
namespace azurerender::ui {
enum class Language { Chinese = 0, English = 1 };
void setLanguage(Language language);
Language language();
// UI-thread only. Returned catalog text remains valid across language changes.
// Unknown text is returned untouched; callers must not pass transient pointers.
const char* tr(const char* text);
}
