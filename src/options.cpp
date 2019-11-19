//
// Copyright Copyright 2009-2019, AMT – The Association For Manufacturing Technology (“AMT”)
// All rights reserved.
//
//    Licensed under the Apache License, Version 2.0 (the "License");
//    you may not use this file except in compliance with the License.
//    You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//    Unless required by applicable law or agreed to in writing, software
//    distributed under the License is distributed on an "AS IS" BASIS,
//    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//    See the License for the specific language governing permissions and
//    limitations under the License.
//

#define _CRT_SECURE_NO_WARNINGS 1

#include "options.hpp"

#include <sys/stat.h>

#include <ctype.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

using namespace std;

#ifdef _WINDOWS
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif

namespace mtconnect
{
  // For arguments which have no switch char but appear in a special order.
  Option::Option(int order, const char *&charPtr, const char *usage, const char *argDesc,
                 bool required)
      : name_(nullptr),
        charPtrPtr_(&charPtr),
        boolPtr_(nullptr),
        intPtr_(nullptr),
        list_(nullptr),
        type_(eCharacter),
        order_(order),
        required_(required),
        argument_(false),
        ignoreCase_(false),
        switch_(false),
        usage_(usage),
        isSet_(false),
        expand_(false),
        argDesc_(argDesc)
  {
  }

  // For arguments which have no switch char but appear in a special order.
  Option::Option(int order, int &intRef, const char *usage, const char *argDesc, bool required)
      : name_(nullptr),
        charPtrPtr_(nullptr),
        boolPtr_(nullptr),
        intPtr_(&intRef),
        list_(nullptr),
        type_(eInteger),
        order_(order),
        required_(required),
        argument_(false),
        ignoreCase_(false),
        switch_(false),
        usage_(usage),
        isSet_(false),
        expand_(false),
        argDesc_(argDesc)
  {
  }

  // For the rest of the argumets as in a file list.
  Option::Option(list<string> &list, const char *usage, const char *argDesc, bool required,
                 bool expand)
      : name_(nullptr),
        charPtrPtr_(nullptr),
        boolPtr_(nullptr),
        intPtr_(nullptr),
        list_(&list),
        type_(eList),
        order_(-1),
        required_(required),
        argument_(false),
        ignoreCase_(false),
        switch_(false),
        usage_(usage),
        isSet_(false),
        expand_(expand),
        argDesc_(argDesc)
  {
  }

  // Given an agument with a switch char ('-') <name>
  Option::Option(const char *name, const char *&charPtr, const char *usage, const char *argDesc,
                 bool required, bool ignoreCase)
      : name_(name),
        charPtrPtr_(&charPtr),
        boolPtr_(nullptr),
        intPtr_(nullptr),
        list_(nullptr),
        type_(eCharacter),
        order_(-1),
        required_(required),
        argument_(true),
        ignoreCase_(ignoreCase),
        switch_(true),
        usage_(usage),
        isSet_(false),
        expand_(false),
        argDesc_(argDesc)
  {
  }

  // Given an agument with a switch char ('-') <name>
  Option::Option(const char *name, bool &boolRef, const char *usage, bool aArgument,
                 const char *argDesc, bool required, bool ignoreCase)
      : name_(name),
        charPtrPtr_(nullptr),
        boolPtr_(&boolRef),
        intPtr_(nullptr),
        list_(nullptr),
        type_(eBoolean),
        order_(-1),
        required_(required),
        argument_(aArgument),
        ignoreCase_(ignoreCase),
        switch_(true),
        usage_(usage),
        isSet_(false),
        expand_(false),
        argDesc_(argDesc)
  {
  }

  // Given an agument with a switch char ('-') <name>
  Option::Option(const char *name, int &intRef, const char *usage, const char *argDesc,
                 bool required, bool ignoreCase)
      : name_(name),
        charPtrPtr_(nullptr),
        boolPtr_(nullptr),
        intPtr_(&intRef),
        list_(nullptr),
        type_(eInteger),
        order_(-1),
        required_(required),
        argument_(true),
        ignoreCase_(ignoreCase),
        switch_(true),
        usage_(usage),
        isSet_(false),
        expand_(false),
        argDesc_(argDesc)
  {
  }

  // Given an agument with a switch char ('-') <name>
  Option::Option(const char *name, list<string> &list, const char *usage, const char *argDesc,
                 bool required, bool expand, bool ignoreCase)
      : name_(name),
        charPtrPtr_(nullptr),
        boolPtr_(nullptr),
        intPtr_(nullptr),
        list_(&list),
        type_(eList),
        order_(-1),
        required_(required),
        argument_(true),
        ignoreCase_(ignoreCase),
        switch_(true),
        usage_(usage),
        isSet_(false),
        expand_(expand),
        argDesc_(argDesc)
  {
  }

  bool Option::setValue(const char *aCp)
  {
    bool rc = true;

    if (type_ != eList && isSet_)
    {
      if (name_)
        cerr << "Option " << name_ << " is already specified" << endl;
      else
        cerr << "Option is alread specified" << endl;

      return false;
    }

    switch (type_)
    {
      case eInteger:
        if (isdigit(*aCp))
          *(intPtr_) = atoi(aCp);
        else
          rc = false;

        break;

      case eBoolean:
        *(boolPtr_) = (*aCp == 'Y' || *aCp == 'T') ? true : false;
        break;

      case eCharacter:
        *(charPtrPtr_) = aCp;
        break;

      case eList:
        if (expand_)
          expandFiles(aCp);
        else
          list_->push_back(aCp);

        break;

      default:
        cerr << "Bad option type" << endl;
        rc = false;
    }

    if (rc)
      isSet_ = true;

    return rc;
  }

  void Option::expandFiles(const char *fileName)
  {
    if (strchr(fileName, '*') || strchr(fileName, '?'))
    {
#ifdef NOT_DEF
      WIN32_FIND_DATA fileData;
      HANDLE search = FindFirstFile(fileName, &fileData);

      if (search != INVALID_HANDLE_VALUE)
      {
        bool done = false;

        while (!done)
        {
          char *cp = new char[strlen(fileData.cFileName) + 1];
          strcpy(cp, fileData.cFileName);
          list_->append(cp);

          done = !FindNextFile(search, &fileData);
        }

        FindClose(search);
      }

#endif
    }
    else
      list_->push_back(fileName);
  }

  bool Option::operator<(const Option &another) const
  {
    if (!name_ && another.getName())
      return false;
    else if (name_ && !another.getName())
      return true;
    else if (!name_ && !another.getName())
    {
      if (order_ == -1)
        return false;

      if (another.getOrder() == -1)
        return true;

      return order_ < another.getOrder();
    }

    return strcmp(name_, another.getName()) < 0;
  }

  // Accept member-wise copy
  // Option::Option(const Option &aOption)

  //---- Destructor
  Option::~Option()
  {
  }

  // --------- Class OptionList begins here.

  OptionsList::OptionsList()
  {
    unswitched_ = -1;
    ownsOptions_ = false;
    program_ = nullptr;
  }

  OptionsList::OptionsList(Option *optionList[])
  {
    program_ = nullptr;
    unswitched_ = -1;
    ownsOptions_ = false;
    int i = 0;

    while (optionList[i])
      push_back(*optionList[i++]);
  }

  OptionsList::~OptionsList()
  {
  }

  void OptionsList::addOption(Option &option)
  {
    push_back(option);
  }

  int OptionsList::parse(int &argc, const char **argv)
  {
    sort();

    // This assumes that the first option (app name) has already been removed.

    int order = 0, count = 0;

    program_ = "agent";

    Option *opt;
    const char **argp = argv;
    const char *cp;

    while (argc > 0)
    {
      if (**argp == '-')
      {
        cp = (*argp) + 1;
        bool next = false;

        while (*cp && !next)
        {
          if (find(cp, opt))
          {
            count++;

            if (opt->hasArgument())
            {
              getArg(argp, argc, opt, cp);
              next = true;
            }
            else
            {
              if (opt->type_ != Option::eBoolean)
              {
                cerr << "Bad argument definition: " << opt->getName() << endl;
              }
              else if (opt->isSet_)
              {
                cerr << "Option " << opt->getName() << " is already specified" << endl;
                usage();
              }
              else
                *(opt->boolPtr_) = true;

              cp += strlen(opt->getName());
            }

            opt->isSet_ = true;
          }
          else
          {
            cerr << "Bad argument:" << *argp << endl;
            usage();
          }
        }
      }
      else
      {
        if (find(order, opt))
        {
          if (!opt->setValue(*argp))
            usage();

          count++;
        }
        else if (find(-1, opt))
        {
          if (!opt->setValue(*argp))
            usage();

          count++;
        }

        order++;
      }

      argp++;
      argc--;
    }

    for (auto iter = begin(); iter != end(); iter++)
    {
      opt = &(*iter);

      if (opt->isRequired() && !opt->isSet())
      {
        if (opt->getName())
          cerr << "Required option -" << opt->getName() << " is not specified" << endl;
        else
          cerr << "Required option <" << opt->getArgDesc() << "> is not specified" << endl;

        usage();
      }
    }

    return count;
  }

  void OptionsList::getArg(const char **&argv, int &argc, Option *option, const char *aAt)
  {
    const char *cp = aAt + strlen(option->getName());

    if (*cp == '\0')
    {
      ++argv;
      cp = *argv;
      argc--;

      if (argc < 1 || *cp == '-')
      {
        cerr << "Argument required for -" << option->getName() << endl;
        usage();
      }
    }

    if (!option->setValue(cp))
      usage();
  }

  void OptionsList::usage()
  {
    size_t len;
    char buffer[1024] = {0}, *cp = nullptr;

    cp = buffer;
    len = sprintf(buffer, "Usage: %s ", program_);
    cp += len;

    bool hasSimpleFlags = false;

    for (auto iter = begin(); iter != end(); iter++)
    {
      Option *opt = &(*iter);

      if (opt->getName() && !opt->hasArgument() && (len = strlen(opt->getName())) == 1)
      {
        hasSimpleFlags = true;
        break;
      }
    }

    if (hasSimpleFlags)
    {
      *cp++ = '[';
      *cp++ = '-';

      for (auto iter = begin(); iter != end(); iter++)
      {
        Option *opt = &(*iter);

        if (opt->getName() && !opt->hasArgument() && (len = strlen(opt->getName())) == 1)
        {
          strcpy(cp, opt->getName());
          cp += len;
        }
      }

      *cp++ = ']';
    }

    char staging[128] = {0}, *cp2 = nullptr;

    for (auto iter = begin(); iter != end(); iter++)
    {
      Option *opt = &(*iter);

      if (opt->getName() && !opt->hasArgument() && (len = strlen(opt->getName())) == 1)
        continue;

      *cp++ = ' ';

      cp2 = staging;

      if (!opt->isRequired())
        *cp2++ = '[';

      if (opt->getType() == Option::eList)
        *cp2++ = '{';

      if (opt->getName() && !opt->hasArgument() && strlen(opt->getName()) > 1)
      {
        len = sprintf(cp2, "-%s", opt->getName());
        cp2 += len;
      }
      else if (opt->getName() && opt->hasArgument())
      {
        len = sprintf(cp2, "-%s <%s>", opt->getName(), opt->getArgDesc());
        cp2 += len;
      }
      else if (!opt->getName())
      {
        len = sprintf(cp2, "<%s>", opt->getArgDesc());
        cp2 += len;
      }

      if (opt->getType() == Option::eList)
      {
        *cp2++ = '}';
        *cp2++ = '.';
        *cp2++ = '.';
        *cp2++ = '.';
      }

      if (!opt->isRequired())
        *cp2++ = ']';

      *cp2 = '\0';

      if (((cp2 - staging) + (cp - buffer)) > 79)
      {
        *cp++ = '\n';
        *cp = '\0';

        fputs(buffer, stderr);
        strcpy(buffer, "        ");
        cp = buffer + 8;
      }

      strcpy(cp, staging);
      cp += cp2 - staging;
    }

    *cp++ = '\n';
    *cp = '\0';
    fputs(buffer, stderr);

    for (auto iter = begin(); iter != end(); iter++)
    {
      Option *opt = &(*iter);

      if (opt->getName())
      {
        if (opt->hasArgument())
          sprintf(buffer, "-%-2.2s <%s>", opt->getName(), opt->getArgDesc());
        else
          sprintf(buffer, "-%-6.6s", opt->getName());
      }
      else if (opt->getOrder() >= 0)
      {
        sprintf(buffer, "<%s>", opt->getArgDesc());
      }
      else
        sprintf(buffer, "<%s>...", opt->getArgDesc());

      fprintf(stderr, "    %-20.20s : ", buffer);
      const char *cp = opt->getUsage();

      while (*cp != '\0')
      {
        if (*cp == '\n')
        {
          fputc(*cp, stderr);
          int count = 4 + 20 + 1;

          while (count--)
            fputc(' ', stderr);

          fputc('>', stderr);
          fputc(' ', stderr);
        }
        else
        {
          fputc(*cp, stderr);
        }

        cp++;
      }

      fputc('\n', stderr);
    }

    exit(256);
  }

  bool OptionsList::find(const char *optName, Option *&option)
  {
    for (auto iter = begin(); iter != end(); iter++)
    {
      option = &(*iter);
      const char *name = option->getName();

      // Unnamed options are at the end of the list.
      if (!name)
        break;

      size_t len = strlen(name);

      if (option->ignoreCase())
      {
        if (!strncasecmp(optName, name, len))
          return true;
      }
      else if (!strncmp(optName, name, len))
        return true;
    }

    return false;
  }

  bool OptionsList::find(int order, Option *&option)
  {
    for (auto iter = begin(); iter != end(); iter++)
    {
      option = &(*iter);

      // Unnamed options are at the end of the list.
      if (!option->getName() && option->getOrder() == order)
        return true;
    }

    return false;
  }
}  // namespace mtconnect
