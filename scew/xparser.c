/**
 * @file     xparser.c
 * @brief    xparser.h implementation
 * @author   Aleix Conchillo Flaque <aleix@member.fsf.org>
 * @date     Tue Dec 03, 2002 00:21
 *
 * @if copyright
 *
 * Copyright (C) 2002-2008 Aleix Conchillo Flaque
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
 */

#include "xparser.h"

#include "str.h"

#include "xerror.h"

#include <assert.h>


// Private

struct stack_element
{
  scew_element* element;
  struct stack_element* prev;
};

// Expat callback for XML declaration.
static void expat_xmldecl_handler_ (void *data,
                                    XML_Char const *version,
                                    XML_Char const *encoding,
                                    int standalone);

// Expat callback for starting elements.
static void expat_start_handler_ (void *data,
                                  XML_Char const *name,
                                  XML_Char const **attr);

// Expat callback for ending elements.
static void expat_end_handler_ (void *data, XML_Char const *name);

// Expat callback for element contents.
static void expat_char_handler_ (void *data, XML_Char const *str, int len);

// Tells Expat parser to stop due to a SCEW error.
static void stop_expat_parsing_ (scew_parser *parser, scew_error error);

// Creates a new tree for the given parser (if not created already).
static scew_tree* create_tree_ (scew_parser *parser);

// Creates a new element with the given name and attributes.
static scew_element* create_element_ (XML_Char const *name,
                                      XML_Char const **attrs);

// Pushes an element into the stack returning the pushed element.
static stack_element* parser_stack_push_ (scew_parser *parser,
                                          scew_element *element);

// Pops an element from the stack returning the new top element (not
// the actual top).
static scew_element* parser_stack_pop_ (scew_parser *parser);


// Protected

bool
scew_parser_expat_init_ (scew_parser *parser)
{
  assert (parser != NULL);

  parser->parser = XML_ParserCreate (NULL);

  bool result = (parser->parser != NULL);

  if (result)
    {
      XML_SetXmlDeclHandler (parser->parser, expat_xmldecl_handler_);
      XML_SetElementHandler (parser->parser,
                             expat_start_handler_,
                             expat_end_handler_);
      XML_SetCharacterDataHandler (parser->parser, expat_char_handler_);

      // Data to be passed to all handlers is the SCEW parser.
      XML_SetUserData (parser->parser, parser);
    }
  else
    {
      scew_error_set_last_error_ (scew_error_no_memory);
    }

  return result;
}

void
scew_parser_stack_free_ (scew_parser *parser)
{
  if (parser != NULL)
    {
      scew_element *element = parser_stack_pop_ (parser);
      while (element != NULL)
        {
          element = parser_stack_pop_ (parser);
        }
    }
}


// Private (handlers)

void
expat_xmldecl_handler_ (void *data,
                        XML_Char const *version,
                        XML_Char const *encoding,
                        int standalone)
{
  scew_parser *parser = (scew_parser *) data;

  if (parser == NULL)
    {
      stop_expat_parsing_ (parser, scew_error_internal);
      return;
    }

  parser->tree = create_tree_ (parser);
  if (parser->tree == NULL)
    {
      stop_expat_parsing_ (parser, scew_error_no_memory);
      return;
    }

  if (version != NULL)
    {
      scew_tree_set_xml_version (parser->tree, scew_strdup (version));
    }
  if (encoding != NULL)
    {
      scew_tree_set_xml_encoding (parser->tree, scew_strdup (encoding));
    }

  // We need to add 1 as our standalone enumeration starts at 0. Expat
  // returns -1, 0 or 1.
  scew_tree_set_xml_standalone (parser->tree, standalone + 1);
}

void
expat_start_handler_ (void *data,
                      XML_Char const *name,
                      XML_Char const **attrs)
{
  scew_parser *parser = (scew_parser *) data;

  if (parser == NULL)
    {
      stop_expat_parsing_ (parser, scew_error_internal);
      return;
    }

  // Create element
  scew_element *element = create_element_ (name, attrs);
  if (element == NULL)
    {
      stop_expat_parsing_ (parser, scew_error_no_memory);
      return;
    }

  // Add the element to its parent (if any)
  if (parser->current != NULL)
    {
      (void) scew_element_add_element (parser->current, element);
    }

  // Push element onto the stack
  stack_element *stack = parser_stack_push_ (parser, element);
  if (stack == NULL)
    {
      stop_expat_parsing_ (parser, scew_error_no_memory);
      return;
    }

  parser->current = element;
}

void
expat_end_handler_ (void *data, XML_Char const *elem)
{
  scew_parser *parser = (scew_parser *) data;

  scew_element *current = parser->current;

  if ((parser == NULL) || (current == NULL))
    {
      stop_expat_parsing_ (parser, scew_error_internal);
      return;
    }

  XML_Char const *contents = scew_element_contents (current);

  // Trim element contents if necessary
  if (parser->ignore_whitespaces && (contents != NULL))
    {
      // We use the internal const pointer for performance reasons
      scew_strtrim ((XML_Char *) contents);
      if (scew_strlen (contents) == 0)
        {
          scew_element_free_contents (current);
        }
    }

  // Go back to the previous element
  parser->current = parser_stack_pop_ (parser);

  // If there are no more elements (root node) we set the root
  // element.
  if (parser->current == NULL)
    {
      parser->tree = create_tree_ (parser);
      if (parser->tree == NULL)
        {
          stop_expat_parsing_ (parser, scew_error_no_memory);
          return;
        }
      (void) scew_tree_set_root_element (parser->tree, current);
    }
}

void
expat_char_handler_ (void *data, XML_Char const *str, int len)
{
  scew_parser *parser = (scew_parser *) data;

  scew_element *current = parser->current;

  if ((parser == NULL) || (current == NULL))
    {
      stop_expat_parsing_ (parser, scew_error_internal);
      return;
    }

  // Get size of current contents
  unsigned int total_old = 0;
  XML_Char const *contents = scew_element_contents (current);
  if (contents != NULL)
    {
      total_old = scew_strlen (contents);
    }

  // Calculate new size and allocate enough space
  unsigned int total = (total_old + len + 1) * sizeof (XML_Char);
  XML_Char *new_contents = calloc (total, 1);

  // Copy old contents (if any) and concatenate new one
  if (contents != NULL)
    {
      scew_strcpy (new_contents, contents);
    }
  scew_strncat (new_contents, str, len);

  scew_element_set_contents (current, new_contents);
}


// Private (miscellaneous)

void
stop_expat_parsing_ (scew_parser *parser, scew_error error)
{
  XML_StopParser (parser->parser, XML_FALSE);
  scew_error_set_last_error_ (error);
}

scew_tree*
create_tree_ (scew_parser *parser)
{
  scew_tree *tree = parser->tree;

  if (tree == NULL)
    {
      tree = scew_tree_create ();
    }

  return tree;
}

scew_element*
create_element_ (XML_Char const *name, XML_Char const **attrs)
{
  scew_element *element = scew_element_create (name);

  for (unsigned int i = 0; (element != NULL) && (attrs[i] != NULL); i += 2)
    {
      scew_attribute *attr = scew_element_add_attribute_pair (element,
                                                              attrs[i],
                                                              attrs[i + 1]);
      if (attr == NULL)
        {
          scew_element_free (element);
          element = NULL;
        }
    }

  return element;
}


// Private (stack)

stack_element*
parser_stack_push_ (scew_parser *parser, scew_element *element)
{
  assert (element != NULL);

  stack_element *new_elem = calloc (1, sizeof (stack_element));

  if (new_elem != NULL)
    {
      new_elem->element = element;
      if (parser->stack != NULL)
        {
	  new_elem->prev = parser->stack;
        }
      parser->stack = new_elem;
    }

  return new_elem;
}

scew_element*
parser_stack_pop_ (scew_parser *parser)
{
  assert (stack != NULL);

  stack_element *element = parser->stack;
  if (element != NULL)
    {
      parser->stack = element->prev;
      free (element);
    }

  return (parser->stack != NULL) ? parser->stack->element : NULL;
}
