#ifndef hpp_CPP_ArgumentParser_CPP_hpp
#define hpp_CPP_ArgumentParser_CPP_hpp

// We need FastString
#include "Strings/Strings.hpp"
// We need IndexList too
#include "Container/Container.hpp"
// We need RobinHoodHashTable
#include "Hash/RobinHoodHashTable.hpp"

/** This is where command line argument parser is declared.
    @sa Argument::parse() */
namespace Arguments
{
#ifndef TRANS
  #define TRANS(X) X
#endif
    /** The string class we are using */
    typedef Strings::FastString String;
    /** The read only string class we are using */
    typedef Strings::VerySimpleReadOnlyString ROString;

    /** An command line argument descriptor */
    struct Descriptor
    {
        /** The help text that's displayed when asking program's usage */
        ROString    helpText;
        /** Short trigger, typically what's after '-'. If not provided, it's deduced from the long trigger (first letter if available) */
        String      shortTrigger;
        /** The long trigger (if provided), typically what's after '--') */
        String      longTrigger;
        /** The expected values if any */
        int         requiredValues;

        Descriptor(const ROString & help, const String & shortT, const String & longT, const int a) : helpText(help), shortTrigger(shortT), longTrigger(longT), requiredValues(a) {}
    };

    /** The argument base action.
        You're unlikely to use this class, but one of its derivative, based on the action you want to perform when an argument is provided */
    struct BaseAction
    {
        /** This is called when a user-given argument matches one of the available descriptors.
            @param desc         The descriptor that matched the argument
            @param values       If the descriptor has a value, the value itself, as a String *
            @param valuesCount  The number of item in the values array
            @return An empty string upon success, else the text returned will be output to the user and the program will terminate */
        virtual String match(const Descriptor & desc, const String * value, const int valuesCount) = 0;

        /** Check if the action is prioritary. Function based actions are typically not */
        virtual int priorityLevel() const { return 1; }
        size_t opaque;

        virtual ~BaseAction() {}
    };

    /** An argument base action based on reference. The action to perform is modifying a referenced value */
    template <typename T> struct RefAction : public BaseAction
    {
        T & ref;
        virtual String match(const Descriptor & desc, const String * values, const int valuesCount)
        {
            if (valuesCount != desc.requiredValues || valuesCount != 1 || !values) return TRANS("Invalid number of arguments for option: ") + TRANS(desc.longTrigger);
            // If the compiler stops here, it's because there is no known way to convert from a String to your type T. You'll have to provide your own specialization of this template for this
            ref = (T)values[0];
            return "";
        }
        /** Construction set the reference */
        RefAction(T & obj) : ref(obj) {}
    };
    /** The specific case for boolean, where only the presence of the argument is enough to enable the argument */
    template <> struct RefAction<bool> : public BaseAction
    {
        bool & ref;
        virtual String match(const Descriptor & desc, const String * values, const int valuesCount)
        {
            if (valuesCount != desc.requiredValues || !valuesCount)
            {
                if (desc.requiredValues <= 1 && !valuesCount)
                {   // Presence of the flag is enough to enable the argument
                    ref = true;
                    return "";
                }
            }
            if (desc.requiredValues > 1 && (valuesCount != 1 || !values)) return TRANS("Invalid number of arguments for option: ") + TRANS(desc.longTrigger);
            ref = values[0] == "y" || values[0] == "Y" || values[0] == "yes" || values[0] == "true";
            return "";
        }
        RefAction<bool>(bool & ref) : ref(ref) {}
    };
    /** An argument base action based on reference. The action to perform is modifying a referenced value.
        This is specific for unsigned integer based values and allow hexadecimal representation for the argument */
    template <> struct RefAction<uint32> : public BaseAction
    {
        uint32 & ref;
        virtual String match(const Descriptor & desc, const String * values, const int valuesCount)
        {
            if (valuesCount != desc.requiredValues || valuesCount != 1 || !values) return TRANS("Invalid number of arguments for option: ") + TRANS(desc.longTrigger);
            int consumed = 0;
            ref = (uint32)values[0].parseInt(0, &consumed);
            return consumed ? "" : TRANS("Could not understand the given value for option: ") + TRANS(desc.longTrigger) + " => " + values[0];
        }
        /** Construction set the reference */
        RefAction(uint32 & obj) : ref(obj) {}
    };
    /** An argument base action based on reference. The action to perform is modifying a referenced value.
        This is specific for unsigned integer based values and allow hexadecimal representation for the argument */
    template <> struct RefAction<uint64> : public BaseAction
    {
        uint64 & ref;
        virtual String match(const Descriptor & desc, const String * values, const int valuesCount)
        {
            if (valuesCount != desc.requiredValues || valuesCount != 1 || !values) return TRANS("Invalid number of arguments for option: ") + TRANS(desc.longTrigger);
            int consumed = 0;
            ref = (uint64)values[0].parseInt(0, &consumed);
            return consumed ? "" : TRANS("Could not understand the given value for option: ") + TRANS(desc.longTrigger) + " => " + values[0];
        }
        /** Construction set the reference */
        RefAction(uint64 & obj) : ref(obj) {}
    };
    /** An argument base action based on reference. The action to perform is modifying a referenced value.
        This is specific for unsigned integer based values and allow hexadecimal representation for the argument */
    template <> struct RefAction<int32> : public BaseAction
    {
        int32 & ref;
        virtual String match(const Descriptor & desc, const String * values, const int valuesCount)
        {
            if (valuesCount != desc.requiredValues || valuesCount != 1 || !values) return TRANS("Invalid number of arguments for option: ") + TRANS(desc.longTrigger);
            int consumed = 0;
            ref = (int32)values[0].parseInt(0, &consumed);
            return consumed ? "" : TRANS("Could not understand the given value for option: ") + TRANS(desc.longTrigger) + " => " + values[0];
        }
        /** Construction set the reference */
        RefAction(int32 & obj) : ref(obj) {}
    };
    /** An argument base action based on reference. The action to perform is modifying a referenced value.
        This is specific for unsigned integer based values and allow hexadecimal representation for the argument */
    template <> struct RefAction<int64> : public BaseAction
    {
        int64 & ref;
        virtual String match(const Descriptor & desc, const String * values, const int valuesCount)
        {
            if (valuesCount != desc.requiredValues || valuesCount != 1 || !values) return TRANS("Invalid number of arguments for option: ") + TRANS(desc.longTrigger);
            int consumed = 0;
            ref = (int64)values[0].parseInt(0, &consumed);
            return consumed ? "" : TRANS("Could not understand the given value for option: ") + TRANS(desc.longTrigger) + " => " + values[0];
        }
        /** Construction set the reference */
        RefAction(int64 & obj) : ref(obj) {}
    };
    struct CatchAll : public BaseAction
    {
        String & ref;
        virtual String match(const Descriptor & desc, const String * values, const int valuesCount)
        {
            if (desc.requiredValues == 1)
            {
                if (valuesCount == 1) ref = values[0];
                else return TRANS("Invalid text found after: ") + values[0];
            }
            else
            {   // We need to merge all text with space
                for (int i = 0; i < valuesCount; i++)
                {
                    if (i) ref += " ";
                    ref += values[i];
                }
            }
            return "";
        }
        /** Construction set the reference */
        CatchAll(String & obj) : ref(obj) {}
    };


    namespace Private
    {
        template <typename Function> struct ArgType;
        // Specialization for 1 argument function
        // Some compilers have trouble with function type as template parameters, sometimes it's a reference, sometimes it's a pointer and some use value too
        template <class Ret, class Arg> struct ArgType<Ret(*)(Arg)> { typedef Arg Type; };
        template <class Ret, class Arg> struct ArgType<Ret(&)(Arg)> { typedef Arg Type; };
        template <class Ret, class Arg> struct ArgType<Ret(Arg)>    { typedef Arg Type; };

        template <class Ret, class Arg0, class Arg1> struct ArgType<Ret(*)(Arg0, Arg1)> { typedef Arg0 Type0; typedef Arg1 Type1; };
        template <class Ret, class Arg0, class Arg1> struct ArgType<Ret(&)(Arg0, Arg1)> { typedef Arg0 Type0; typedef Arg1 Type1; };
        template <class Ret, class Arg0, class Arg1> struct ArgType<Ret(Arg0, Arg1)>    { typedef Arg0 Type0; typedef Arg1 Type1; };

    }



    /** An argument action that calls a function when the argument is received.
        @param Func     The function to call. It must return a String (or a type convertible to String like const char*) and have a single argument that's supported by String conversion (like uint64, int, float, double, etc...) */
    template <typename Func> struct CallAction : public BaseAction
    {
        Func func;
        virtual String match(const Descriptor & desc, const String * values, const int valuesCount)
        {
            if (valuesCount != desc.requiredValues || valuesCount != 1 || !values) return TRANS("Invalid number of arguments for option: ") + TRANS(desc.longTrigger);
            // If the compiler stops here, it's likely because there is no known conversion from String to your function's parameter
            return func((typename Private::ArgType<Func>::Type)values[0]);
        }
        /** Check if the action is prioritary. Function based actions are typically not */
        virtual int priorityLevel() const { return 0; }

        CallAction(Func f) : func(f) {}
    };
    /** An argument action that calls a function when the argument is received.
        @param Func     The function to call. It must return a String (or a type convertible to String like const char*) and have a single argument that's supported by String conversion (like uint64, int, float, double, etc...) */
    template <typename Func> struct CallAction2 : public BaseAction
    {
        Func func;
        virtual String match(const Descriptor & desc, const String * values, const int valuesCount)
        {
            if (valuesCount > desc.requiredValues || valuesCount < 1 || !values) return TRANS("Invalid number of arguments for option: ") + TRANS(desc.longTrigger);
            // If the compiler stops here, it's likely because there is no known conversion from String to your function's parameter
            return func((typename Private::ArgType<Func>::Type0)values[0], (typename Private::ArgType<Func>::Type1)values[1]);
        }
        /** Check if the action is prioritary. Function based actions are typically not */
        virtual int priorityLevel() const { return 0; }

        CallAction2(Func f) : func(f) {}
    };



    /** The function without any argument */
    typedef String (*NoArgFunc)();
    /** An argument action that calls a function when the argument is received.
        @param Func     The function to call. It must return a String (or a type convertible to String like const char*) and no argument */
    struct CallActionNoArg : public BaseAction
    {
        NoArgFunc func;
        virtual String match(const Descriptor & desc, const String * values, const int valuesCount)
        {
            if (valuesCount != desc.requiredValues || valuesCount) return TRANS("Invalid number of arguments for option: ") + TRANS(desc.longTrigger);
            // If the compiler stops here, it's likely because there is no known conversion from String to your function's parameter
            return func();
        }
        /** Check if the action is prioritary. Function based actions are typically not */
        virtual int priorityLevel() const { return 0; }

        CallActionNoArg(NoArgFunc f) : func(f) {}
    };




    /** This is the main argument core processing.
        You usually need to declare the arguments you expect to receive via the numerous @sa declare() functions.
        This is done upon a singleton, so you don't need to bother handling the lifetime of those.
        Then you'll simply call parse() method and it'll perform its magic.

        Example code:
        @code
            int someValue = 42; // Default

            String myFunc(double pi)
            {
                if (pi != 3.14) return "Are you sure you intend to collapse the complete universe ?";
                return "";
            }

            String anotherFunc()
            {
                return "Too late, sorry guy";
            }

            SomeIntrospectableClass window;

            int main(int argc, char ** argv)
            {
                Arguments::declare(someValue, "Change the answer to the question of life", "answer");
                Arguments::declare(myFunc, "It's the pi-killer, set pi as the given argument", "set-pi", "t");
                Arguments::declare(anotherFunc, "Disarm bomb", "dry-run");
                Arguments::declare(window, "title", "Set the title", "title");

                String error = Arguments::parse(argc, argv);
                if (error) fprintf(stderr, "%s\n", (const char*)error);

                return error ? 1 : 0;
            }
        @endcode

        The example code above, when run, produces:
        @verbatim
            $ ./myCode -h
            Usage is: ./myCode [options]
            Options:
                --help or -h           Get this help message
                --answer or -a arg     Change the answer to the question of life
                --set-pi or -t arg     It's the pi-killer, set pi as the given argument
                --dry-run or -d        Disarm bomb
                --title -i arg         Set the title

            $ ./myCode -a 41
            $ ./myCode --set-pi 4
            Are you sure you intend to collapse the complete universe ?
            $ ./myCode -d uhuh
            Too late, sorry guy
            Error parsing the argument (option not found, use -h for a list of options): "uhuh"
            $ ./myCode -i "Hello"
            $ ./myCode -k Blah
            Error parsing the argument (option not found, use -h for a list of options): "-k"
            $
        @endverbatim
        */
    class Core
    {
        /** The array of description for each possible arguments */
        Container::NotConstructible<Descriptor>::IndexList descriptors;
        Container::NotConstructible<BaseAction>::IndexList actions;
        /** The mapping between short option and the descriptor */
        typedef Container::RobinHoodHashTable<BaseAction *, String, Container::StringHashingPolicy<String> > ActionTable;
        /** The mapping between short trigger and the descriptor and action */
        ActionTable shortTrigger;
        /** The mapping between long trigger and the descriptor and action */
        ActionTable longTrigger;
        /** The program output header, if any required */
        ROString header;

        /** Helper method to register descriptor and actions */
        void internalReg(Descriptor * desc, BaseAction * action)
        {
            size_t descIndex = descriptors.getSize();
            descriptors.Append(desc);
            actions.Append(action);
            action->opaque = descIndex;
            // That's the declare last, and this one has no trigger, obviously
            if (!desc->longTrigger) return;
            // Then register the triggers
            longTrigger.reliableStoreValue(desc->longTrigger, action);
            // Then also register the short trigger if provided, else we need to allocate one
            // Need to allocate a short trigger here
            // So, first check if the first caracter of the long trigger is available
            for (int i = 0; !desc->shortTrigger && i < desc->longTrigger.getLength(); i++)
            {
                String key(desc->longTrigger[i]);
                if (!shortTrigger.getValue(key))
                    // Found a free character, let's use it
                    desc->shortTrigger = key;
            }
            if (desc->shortTrigger)
                shortTrigger.reliableStoreValue(desc->shortTrigger, action);
        }

        String internalTrigger(const Strings::StringArray & arguments, const bool hasCatchAll, int currentPriority)
        {

            for (size_t i = 1; i < arguments.getSize(); i++)
            {
                const String & arg = arguments[i];

                // Check if it's a short trigger or long trigger
                if (arg[0] == '-')
                {
                    const bool shortT = arg[1] != '-';
                    const String & filteredArg = arg.midString(shortT ? 1 : 2, arg.getLength());
                    BaseAction ** action = shortT ? shortTrigger.getValue(filteredArg) : longTrigger.getValue(filteredArg);
                    if (!action) return TRANS("Error parsing the argument (option not found, use -h for a list of options): ") + filteredArg;
                    // Check the value count now
                    const Descriptor & desc = descriptors[(*action)->opaque];
                    if ((*action)->priorityLevel() != currentPriority) { i += desc.requiredValues; continue; }

                    // Check if we can have enough values (at least one, let action deal with missing values if mandatory)
                    if ((desc.requiredValues ? 1 : 0) + i > arguments.getSize()) return TRANS("Not enough argument for option (use -h for usage): ") + filteredArg;
                    // Need to build the arguments array
                    size_t doneValues = 0;
                    String * argArray = new String[desc.requiredValues]; 
                    for (size_t a = 0; a < desc.requiredValues; a++) 
                    {
                        if (i + 1 + a >= arguments.getSize()) break;
                        const String & tmpArg = arguments[i+1+a];
                        if (tmpArg[0] == '-') break;
                        argArray[a] = tmpArg;
                        doneValues++;
                    }

                    String ret = (*action)->match(desc, argArray, min((int)doneValues, desc.requiredValues));
                    delete[] argArray;
                    if (ret) return ret;

                    i += doneValues;
                } else if (hasCatchAll && actions[descriptors.getSize() - 1].priorityLevel() == currentPriority)
                {
                    String * argArray = new String[arguments.getSize() - i]; 
                    for (size_t a = 0; a < arguments.getSize() - i; a++) 
                    {
                        const String & tmpArg = arguments[i+a];
                        argArray[a] = tmpArg;
                    }
                    String ret = actions[descriptors.getSize() - 1].match(descriptors[descriptors.getSize() - 1], argArray, (int)(arguments.getSize() - i));
                    delete[] argArray;
                    return ret;
                }
                else return TRANS("Error parsing the argument (option not found, use -h for a list of options): ") + arg;
            }
            return "";
        }

        String internalParse(const Strings::StringArray & arguments)
        {
            // The program name is usually the first item to parse, so let's do that
            if (arguments.getSize() < 1) return TRANS("Invalid usage of this method");
            //const String & programName = arguments[0];
            const bool hasCatchAll = descriptors.getSize() ? !descriptors[descriptors.getSize() - 1].longTrigger : false;
            if (arguments.getSize() == 1) return helpMessage();
            // Try high priority items first
            String ret = internalTrigger(arguments, hasCatchAll, 1);
            if (ret) return ret;
            // Then try low priority 
            return internalTrigger(arguments, hasCatchAll, 0);
        }


    public:
        /** The basic instance, although it's never called like this */
        static Core & getInstance() { static Core c; return c; }
        /** Register a descriptor and action
            @param desc     A pointer to a new allocated Descriptor that's owned
            @param action   A pointer to a new allocated BaseAction that's owned */
        static void registerDescriptorAndAction(Descriptor * desc, BaseAction * action)
        {
            Core & t = getInstance();
            t.internalReg(desc, action);
        }
        /** Parse the given argument list and execute actions */
        static String parse(const Strings::StringArray & arguments)
        {
            return getInstance().internalParse(arguments);
        }
        /** Parse the given argument list and execute actions */
        static inline String parse(const int argc, const char ** argv) { return parse(Strings::StringArray(argc, argv)); }


        /** Set the program header */
        static void setProgramHeader(const ROString & header) { getInstance().header = header; }

        /** Generate the usual helpful message user expects when entering -h */
        static String helpMessage()
        {
            Core & t = getInstance();
            size_t optionsCount = t.descriptors.getSize();
            String out = t.header ? Strings::convert(t.header) : String();

            out += String::Print(TRANS("Usage is: %s [options] %s\n"), Platform::getProcessName(), t.descriptors[optionsCount-1].longTrigger ? "" : t.descriptors[optionsCount-1].helpText.getData());
            if (optionsCount) out += TRANS("Options:\n");
            for (size_t i = 0; i < optionsCount; i++)
            {
                const Descriptor & desc = t.descriptors[i];
                if (desc.longTrigger)
                    out += String::Print("\t--%s%s%s\t\t%s\n", (const char*)desc.longTrigger, desc.shortTrigger ? (const char*)String::Print(TRANS(" or -%s"), (const char*)desc.shortTrigger) : "", desc.requiredValues ? (const char*)TRANS(" arg") : "", desc.helpText.getData());
            }
            return out + "\n";
        }

    private:
        /** Prevent construction without singleton. Default to 16 arguments, but it's enlarged if required */
        Core() : shortTrigger(16), longTrigger(16)
        {
            // Register the help function
            internalReg(new Descriptor("Get this help message", "h", "help", 0), new CallActionNoArg(helpMessage));
        }
    };

    /** Register actions and description for it.
        This is used for mapping an argument to an instance, in effect, if the argument is found, it changes the instance value.
        @param ref                  The reference on the value to change.
        @param helpText             The text to display for this argument when the user use --help or -h
        @param longTrigger          The long trigger text (usually what's after -- in the argument help description)
        @param shortTrigger         The short trigger text (usually what's after - in the argument help description). If ommited, the system allocate one for you
        @param numberOfArguments    The number of arguments expected for this option. This is typically 1 but for bool type, it can be 0 */
    template <typename T>
    inline void declare(T & ref, const char * helpText, const char * longTrigger, const char * shortTrigger = 0, const int numberOfArguments = 1)
    {
        // First create the descriptor
        Descriptor * desc = new Descriptor(helpText, shortTrigger ? shortTrigger : "", longTrigger, numberOfArguments);
        Core::registerDescriptorAndAction(desc, new RefAction<T>(ref));
    }
    // Overload for bool to avoid specifying a value
    inline void declare(bool & ref, const char * helpText, const char * longTrigger, const char * shortTrigger = 0)
    {
        // First create the descriptor
        Descriptor * desc = new Descriptor(helpText, shortTrigger ? shortTrigger : "", longTrigger, 0);
        Core::registerDescriptorAndAction(desc, new RefAction<bool>(ref));
    }


    /** Register actions and description for it.
        This is used for mapping an argument to an function, in effect, if the argument is found, it calls the given function with the argument's value.
        @note There is always one argument when using this function.

        @param func                 The function to call. The function signature should be "String Func(T)" with T selected as one type String can be converted to.
        @param helpText             The text to display for this argument when the user use --help or -h
        @param longTrigger          The long trigger text (usually what's after -- in the argument help description)
        @param shortTrigger         The short trigger text (usually what's after - in the argument help description). If ommited, the system allocate one for you */
    template <typename T>
    inline void declare(String (&func)(T), const char * helpText, const char * longTrigger, const char * shortTrigger = 0)
    {
        // First create the descriptor
        Descriptor * desc = new Descriptor(helpText, shortTrigger ? shortTrigger : "", longTrigger, 1);
        Core::registerDescriptorAndAction(desc, new CallAction<String(&)(T)>(func));
    }

    /** Register actions and description for it.
        This is used for mapping an argument to an function, in effect, if the argument is found, it calls the given function with the argument's value.
        @note There is always one argument when using this function.

        @param func                 The function to call. The function signature should be "String Func(T)" with T selected as one type String can be converted to.
        @param helpText             The text to display for this argument when the user use --help or -h
        @param longTrigger          The long trigger text (usually what's after -- in the argument help description)
        @param shortTrigger         The short trigger text (usually what's after - in the argument help description). If ommited, the system allocate one for you */
    template <typename T, typename U>
    inline void declare(String (&func)(T, U), const char * helpText, const char * longTrigger, const char * shortTrigger = 0)
    {
        // First create the descriptor
        Descriptor * desc = new Descriptor(helpText, shortTrigger ? shortTrigger : "", longTrigger, 2);
        Core::registerDescriptorAndAction(desc, new CallAction2<String(&)(T, U)>(func));
    }

    // template <typename T, typename Arg = typename Private::ArgTypeRef<T>::Type>
    // inline void declare(T & func, const char * helpText, const char * longTrigger, const char * shortTrigger = 0)
    // {
    //     // First create the descriptor
    //     Descriptor * desc = new Descriptor(helpText, shortTrigger ? shortTrigger : "", longTrigger, 1);
    //     Core::registerDescriptorAndAction(desc, new CallAction<T>(func));
    // }
    /** Register actions and description for it.
        This is used for mapping an argument to an function, in effect, if the argument is found, it calls the given function.
        There is no value associated with this argument

        @param func                 The function to call. The function signature should be "String Func(T)" with T selected as one type String can be converted to.
        @param helpText             The text to display for this argument when the user use --help or -h
        @param longTrigger          The long trigger text (usually what's after -- in the argument help description)
        @param shortTrigger         The short trigger text (usually what's after - in the argument help description). If ommited, the system allocate one for you */
    inline void declare(NoArgFunc func, const char * helpText, const char * longTrigger, const char * shortTrigger = 0)
    {
        // First create the descriptor
        Descriptor * desc = new Descriptor(helpText, shortTrigger ? shortTrigger : "", longTrigger, 0);
        Core::registerDescriptorAndAction(desc, new CallActionNoArg(func));
    }

    /** This is a specific argument that catch all remaining arguments on the command line. This is usually the last one to specify.
        @param ref                  A reference on the instance to modify
        @param numberOfArguments    If provided can be used to check consistancy. If 0, all remaining arguments are joined with space */
    inline void declareLast(String & ref, const char * helpText, const int numberOfArguments = 0)
    {
        // First create the descriptor
        Descriptor * desc = new Descriptor(helpText, "", "", 0);
        Core::registerDescriptorAndAction(desc, new CatchAll(ref));
    }

    /** Inject the parse method here to avoid dealing with Core stuff anywhere else */
    inline String parse(const int argc, const char ** argv) { return Core::parse(argc, argv); }
}


#endif
