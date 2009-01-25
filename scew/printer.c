/**
 * @file     printer.c
 * @brief    printer.h implementation
 * @author   Aleix Conchillo Flaque <aleix@member.fsf.org>
 * @date     Fri Jan 16, 2009 22:38
 *
 * @if copyright
 *
 * Copyright (C) 2003-2009 Aleix Conchillo Flaque
 *
 * SCEW is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * SCEW is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 * @endif
 **/

#include "printer.h"

#include "xerror.h"

#include "str.h"

#include <assert.h>


/* Private */

#define STR_XML_        _XT("xml")
#define STR_VERSION_    _XT("version")
#define STR_ENCODING_   _XT("encoding")
#define STR_STANDALONE_ _XT("standalone")
#define STR_YES_        _XT("yes")
#define STR_NO_         _XT("no")

enum
  {
    DEFAULT_INDENT_SPACES_ = 3  /**< Default number of indent spaces */
  };

struct scew_printer
{
  scew_bool indented;
  unsigned int indent;
  unsigned int spaces;
  scew_writer *writer;
};

static scew_bool print_pi_start_ (scew_printer *printer, XML_Char const *pi);
static scew_bool print_pi_end_ (scew_printer *printer);
static scew_bool print_attribute_ (scew_printer *printer,
                                   XML_Char const* name,
                                   XML_Char const* value);
static scew_bool print_eol_ (scew_printer *printer);
static scew_bool print_indent_ (scew_printer *printer);
static scew_bool print_element_start_ (scew_printer *printer,
                                       scew_element const *element,
                                       scew_bool *closed);
static scew_bool print_element_end_ (scew_printer *printer,
                                     scew_element const *element);


/* Public */

scew_printer*
scew_printer_create (scew_writer *writer)
{
  scew_printer *printer = calloc (1, sizeof (scew_printer));

  if (printer != NULL)
    {
      printer->writer = writer;

      scew_printer_set_indented (printer, SCEW_TRUE);
      scew_printer_set_indentation (printer, DEFAULT_INDENT_SPACES_);
    }

  return printer;
}

void
scew_printer_free (scew_printer *printer)
{
  if (printer != NULL)
    {
      free (printer);
    }
}

scew_writer*
scew_printer_set_writer (scew_printer *printer, scew_writer *writer)
{
  scew_writer *old_writer = NULL;

  assert (printer != NULL);
  assert (writer != NULL);

  old_writer = printer->writer;
  printer->writer = writer;

  return old_writer;
}

void
scew_printer_set_indented (scew_printer *printer, scew_bool indented)
{
  assert (printer != NULL);

  printer->indented = indented;
}

void
scew_printer_set_indentation (scew_printer *printer, unsigned int spaces)
{
  assert (printer != NULL);

  printer->spaces = spaces;
}

scew_bool
scew_printer_print_tree (scew_printer *printer, scew_tree const *tree)
{
  scew_bool result = SCEW_TRUE;
  XML_Char const *version = NULL;
  XML_Char const *encoding = NULL;
  scew_tree_standalone standalone = scew_tree_standalone_unknown;

  assert (printer != NULL);
  assert (tree != NULL);

  version = scew_tree_xml_version (tree);
  encoding = scew_tree_xml_encoding (tree);
  standalone = scew_tree_xml_standalone (tree);

  result = print_pi_start_ (printer, STR_XML_);
  result = result && print_attribute_ (printer, STR_VERSION_, version);

  if (encoding)
    {
      result = result && print_attribute_ (printer, STR_ENCODING_, encoding);
    }

  if (result)
    {
      switch (standalone)
        {
        case scew_tree_standalone_unknown:
          break;
        case scew_tree_standalone_no:
          result = print_attribute_ (printer, STR_STANDALONE_, STR_NO_);
          break;
        case scew_tree_standalone_yes:
          result = print_attribute_ (printer, STR_STANDALONE_, STR_YES_);
          break;
        };
    }

  result = result && print_pi_end_ (printer);

  result = result && scew_printer_print_element (printer, scew_tree_root (tree));

  if (!result)
    {
      scew_error_set_last_error_ (scew_error_io);
    }

  return result;
}

scew_bool
scew_printer_print_element (scew_printer *printer, scew_element const *element)
{
  scew_bool result = SCEW_TRUE;
  scew_bool closed = SCEW_TRUE;

  assert (printer != NULL);
  assert (element != NULL);

  result = print_indent_ (printer);

  result = result && print_element_start_ (printer, element, &closed);

  if (!closed)
    {
      XML_Char const *contents = NULL;

      result = result && scew_printer_print_element_children (printer,
                                                              element);

      contents = scew_element_contents (element);
      if (contents != NULL)
        {
          result = result && scew_writer_write (printer->writer,
                                                contents,
                                                scew_strlen (contents));
        }
      else
        {
          result = result && print_indent_ (printer);
        }
      result = result && print_element_end_ (printer, element);
      result = result && print_eol_ (printer);
    }

  if (!result)
    {
      scew_error_set_last_error_ (scew_error_io);
    }

  return result;
}

scew_bool
scew_printer_print_element_children (scew_printer *printer,
                                    scew_element const  *element)
{
  unsigned int indent = 0;
  scew_list *list = NULL;
  scew_bool result = SCEW_TRUE;

  assert (printer != NULL);
  assert (element != NULL);

  indent = printer->indent;

  list = scew_element_children (element);
  while (result && (list != NULL))
    {
      scew_element *child = scew_list_data (list);

      printer->indent = indent + 1;

      result = scew_printer_print_element (printer, child);
      list = scew_list_next (list);
    }

  printer->indent = indent;

  if (!result)
    {
      scew_error_set_last_error_ (scew_error_io);
    }

  return result;
}


scew_bool
scew_printer_print_element_attributes (scew_printer *printer,
                                      scew_element const *element)
{
  scew_bool result = SCEW_TRUE;
  scew_list *list = NULL;

  assert (printer != NULL);
  assert (element != NULL);

  list = scew_element_attributes (element);
  while (result && (list != NULL))
    {
      scew_attribute *attribute = scew_list_data (list);
      result = scew_printer_print_attribute (printer, attribute);
      list = scew_list_next (list);
    }

  if (!result)
    {
      scew_error_set_last_error_ (scew_error_io);
    }

  return result;
}

scew_bool
scew_printer_print_attribute (scew_printer *printer,
                             scew_attribute const *attribute)
{
  scew_bool result = SCEW_TRUE;

  assert (printer != NULL);
  assert (attribute != NULL);

  result = print_attribute_ (printer,
                             scew_attribute_name (attribute),
                             scew_attribute_value (attribute));

  if (!result)
    {
      scew_error_set_last_error_ (scew_error_io);
    }

  return result;
}



/* Private */

scew_bool
print_pi_start_ (scew_printer *printer, XML_Char const *pi)
{
  static XML_Char const *PI_START = _XT("<?");
  scew_bool result = SCEW_FALSE;

  scew_writer *writer = printer->writer;

  result = scew_writer_write (writer, PI_START, scew_strlen (PI_START));
  result = result && scew_writer_write (writer, pi, scew_strlen (pi));

  return result;
}

scew_bool
print_pi_end_ (scew_printer *printer)
{
  static XML_Char const *PI_END = _XT("?>");
  scew_bool result = SCEW_FALSE;

  result = scew_writer_write (printer->writer, PI_END, scew_strlen (PI_END));
  result = result && print_eol_ (printer);

  return result;
}

scew_bool
print_attribute_ (scew_printer *printer,
                  XML_Char const* name,
                  XML_Char const* value)
{
  scew_bool result = SCEW_FALSE;

  scew_writer *writer = printer->writer;

  result = scew_writer_write (writer, " ", 1);
  result = result && scew_writer_write (writer, name, scew_strlen (name));
  result = result && scew_writer_write (writer, "=\"", 2);
  result = result && scew_writer_write (writer, value, scew_strlen (value));
  result = result && scew_writer_write (writer, "\"", 1);

  return result;
}

scew_bool
print_eol_ (scew_printer *printer)
{
  scew_bool result = SCEW_TRUE;

  assert (printer != NULL);

  if (printer->indented)
    {
      result = scew_writer_write (printer->writer, "\n", 1);
    }

  return result;
}

scew_bool
print_indent_ (scew_printer *printer)
{
  scew_bool result = SCEW_TRUE;

  assert (printer != NULL);

  if (printer->indented)
    {
      unsigned int i = 0;
      unsigned int spaces = printer->indent * printer->spaces;
      for (i = 0; result && (i < spaces); ++i)
        {
          result = scew_writer_write (printer->writer, " ", 1);
        }
    }

  return result;
}

scew_bool
print_element_start_ (scew_printer *printer,
                      scew_element const *element,
                      scew_bool *closed)
{
  static XML_Char const *START = _XT("<");
  static XML_Char const *END_1 = _XT(">");
  static XML_Char const *END_2 = _XT("/>");

  scew_list *list = NULL;
  scew_writer *writer = NULL;
  XML_Char const *name = NULL;
  XML_Char const *contents = NULL;
  scew_bool result = SCEW_TRUE;

  assert (printer != NULL);
  assert (element != NULL);

  writer = printer->writer;

  name = scew_element_name (element);

  result = scew_writer_write (writer, START, 1);
  result = result && scew_writer_write (writer, name, scew_strlen (name));
  result = result && scew_printer_print_element_attributes (printer, element);

  contents = scew_element_contents (element);

  *closed = SCEW_FALSE;
  list = scew_element_children (element);
  if ((contents == NULL) && (list == NULL))
    {
      result = result && scew_writer_write (writer, END_2, 2);
      result = result && print_eol_ (printer);
      *closed = SCEW_TRUE;
    }
  else
    {
      result = result && scew_writer_write (writer, END_1, 1);
      if (contents == NULL)
        {
	  result = result && print_eol_ (printer);
        }
    }

  return result;
}

scew_bool
print_element_end_ (scew_printer *printer, scew_element const *element)
{
  static XML_Char const *START = _XT("</");
  static XML_Char const *END = _XT(">");

  scew_writer *writer = NULL;
  scew_bool result = SCEW_TRUE;

  XML_Char const *name = scew_element_name (element);

  assert (printer != NULL);

  writer = printer->writer;

  result = scew_writer_write (writer, START, 2);
  result = result && scew_writer_write (writer, name, scew_strlen (name));
  result = result && scew_writer_write (writer, END, 1);

  return result;
}
