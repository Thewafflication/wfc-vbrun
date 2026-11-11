// The text of the run-time error messages (Err.Description): the table of
// the reference runtime (its string resources 10000 to 10087, ordered by
// error number).

#include "vbrun_helpers.hpp"

namespace {

struct Message {
    int number;
    const wchar_t* text;
};

constexpr Message kMessages[] = {
    {1, L"Application-defined or object-defined error"},
    {3, L"Return without GoSub"},
    {5, L"Invalid procedure call or argument"},
    {6, L"Overflow"},
    {7, L"Out of memory"},
    {9, L"Subscript out of range"},
    {10, L"This array is fixed or temporarily locked"},
    {11, L"Division by zero"},
    {13, L"Type mismatch"},
    {14, L"Out of string space"},
    {16, L"Expression too complex"},
    {17, L"Can't perform requested operation"},
    {18, L"User interrupt occurred"},
    {20, L"Resume without error"},
    {28, L"Out of stack space"},
    {35, L"Sub or Function not defined"},
    {47, L"Too many DLL application clients"},
    {48, L"Error in loading DLL"},
    {49, L"Bad DLL calling convention"},
    {51, L"Internal error"},
    {52, L"Bad file name or number"},
    {53, L"File not found"},
    {54, L"Bad file mode"},
    {55, L"File already open"},
    {57, L"Device I/O error"},
    {58, L"File already exists"},
    {59, L"Bad record length"},
    {61, L"Disk full"},
    {62, L"Input past end of file"},
    {63, L"Bad record number"},
    {67, L"Too many files"},
    {68, L"Device unavailable"},
    {70, L"Permission denied"},
    {71, L"Disk not ready"},
    {74, L"Can't rename with different drive"},
    {75, L"Path/File access error"},
    {76, L"Path not found"},
    {91, L"Object variable or With block variable not set"},
    {92, L"For loop not initialized"},
    {93, L"Invalid pattern string"},
    {94, L"Invalid use of Null"},
    {96,
     L"Unable to sink events of object because the object is already firing "
     L"events to the maximum number of event receivers that it supports"},
    {97,
     L"Can not call friend function on object which is not an instance of "
     L"defining class"},
    {98,
     L"A property or method call cannot include a reference to a private "
     L"object, either as an argument or as a return value"},
    {321, L"Invalid file format"},
    {322, L"Can't create necessary temporary file"},
    {325, L"Invalid format in resource file"},
    {380, L"Invalid property value"},
    {381, L"Invalid property array index"},
    {382, L"Set not supported at runtime"},
    {383, L"Set not supported (read-only property)"},
    {385, L"Need property array index"},
    {387, L"Set not permitted"},
    {393, L"Get not supported at runtime"},
    {394, L"Get not supported (write-only property)"},
    {422, L"Property not found"},
    {423, L"Property or method not found"},
    {424, L"Object required"},
    {429, L"ActiveX component can't create object"},
    {430,
     L"Class does not support Automation or does not support expected "
     L"interface"},
    {432, L"File name or class name not found during Automation operation"},
    {438, L"Object doesn't support this property or method"},
    {440, L"Automation error"},
    {442,
     L"Connection to type library or object library for remote process has "
     L"been lost. Press OK for dialog to remove reference."},
    {443, L"Automation object does not have a default value"},
    {445, L"Object doesn't support this action"},
    {446, L"Object doesn't support named arguments"},
    {447, L"Object doesn't support current locale setting"},
    {448, L"Named argument not found"},
    {449, L"Argument not optional"},
    {450, L"Wrong number of arguments or invalid property assignment"},
    {451,
     L"Property let procedure not defined and property get procedure did not "
     L"return an object"},
    {452, L"Invalid ordinal"},
    {453, L"Specified DLL function not found"},
    {454, L"Code resource not found"},
    {455, L"Code resource lock error"},
    {457, L"This key is already associated with an element of this collection"},
    {458, L"Variable uses an Automation type not supported in Visual Basic"},
    {459, L"Object or class does not support the set of events"},
    {460, L"Invalid clipboard format"},
    {461, L"Method or data member not found"},
    {462, L"The remote server machine does not exist or is unavailable"},
    {463, L"Class not registered on local machine"},
    {481, L"Invalid picture"},
    {482, L"Printer error"},
    {735, L"Can't save file to TEMP"},
    {744, L"Search text not found"},
    {746, L"Replacements too long"},
};

constexpr const wchar_t* kGeneric =
    L"Application-defined or object-defined error";

}  // namespace

namespace vbrun {

// The message of a Visual Basic error number (the generic one for a number
// that has none; null for 0).
const wchar_t* error_message(const int number) {
    if (number == 0) {
        return nullptr;
    }
    for (const Message& message : kMessages) {
        if (message.number == number) {
            return message.text;
        }
    }
    return kGeneric;
}

}  // namespace vbrun
