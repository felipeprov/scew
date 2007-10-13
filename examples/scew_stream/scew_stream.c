/**
 * @file     scew_stream.c
 * @brief    SCEW usage example
 * @author   Aleix Conchillo Flaque <aleix@member.fsf.org>
 * @date     Sun May 23, 2004 20:58
 *
 * @if copyright
 *
 * Copyright (C) 2004, 2007 Aleix Conchillo Flaque
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
 *
 * This example shows the usage of the SCEW API. It loads an XML file
 * using a one byte buffer (any size can be used). Then, the function
 * scew_parser_load_stream is called until the end of file is
 * reached. While elements are encountered a callback function,
 * previously set, is called each time a complete element is read.
 *
 * Example 1:
 *
 *   <command>command_1</command>                <-- callback called
 *   <command><option>option2</option></command> <-- callback called
 *   <command>command_3</command>                <-- callback called
 *
 * Example 2:
 *
 *   <commands>
 *     <command>command_1</command>
 *     <command>command_2</command>
 *   </commands>                                 <-- callback called
 */

#include <scew/scew.h>

#include <stdio.h>
#include <stdbool.h>

bool
stream_cb (scew_parser *parser)
{
  printf ("SCEW stream callback called!\n");
  return true;
}

int
main (int argc, char *argv[])
{
  if (argc < 2)
    {
      printf ("usage: scew_stream file.xml\n");
      return EXIT_FAILURE;
    }

  FILE *in = fopen (argv[1], "rb");

  if (in == NULL)
    {
      perror ("Unable to open file");
      return EXIT_FAILURE;
    }

  /**
   * Creates an SCEW parser. This is the first function to call.
   */
  scew_parser *parser = scew_parser_create ();

  scew_parser_set_stream_callback (parser, stream_cb);

  int len = 1;
  while (len)
    {
      char buffer;
      len = fread (&buffer, 1, 1, in);

      if (!scew_parser_load_stream (parser, &buffer, 1))
        {
          scew_error code = scew_error_code ();
          printf ("Unable to load stream (error #%d: %s)\n",
                  code,
                  scew_error_string (code));
          if (code == scew_error_expat)
            {
              enum XML_Error expat_code = scew_error_expat_code (parser);
              printf ("Expat error #%d (line %d, column %d): %s\n",
                      expat_code,
                      scew_error_expat_line (parser),
                      scew_error_expat_column (parser),
                      scew_error_expat_string (expat_code));
            }
          return EXIT_FAILURE;
        }
    }

  fclose (in);

  /* Frees the SCEW parser */
  scew_parser_free (parser);

  return 0;
}