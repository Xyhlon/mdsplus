#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>
#include <mdsshr.h>
#include <dcl.h>
#include "dcl_p.h"

static dclDocListPtr dclDocs = 0;

dclDocListPtr mdsdcl_getdocs()
{
  return dclDocs;
}

/*! Free the memory associated with a parameter definition structure.
 \param p [in,out] the address of a pointer to  a dclParameter struct.
*/

void freeParameter(dclParameterPtr * p_in)
{
  dclParameterPtr p = *p_in;
  if (p) {
    int i;
    if (p->name != NULL)
      free(p->name);
    if (p->label != NULL)
      free(p->label);
    if (p->prompt != NULL)
      free(p->prompt);
    if (p->type != NULL)
      free(p->type);
    if (p->restOfLine != NULL)
      free(p->restOfLine);
    for (i = 0; i < p->value_count; i++)
      free(p->values[i]);
    if (p->values)
      free(p->values);
    free(p);
    *p_in = 0;
  }
}

/*! Free memory associated with a dclQualifier struct.
 \param q [in,out] The address of a pointer to a dclQualifier struct
*/

static void freeQualifier(dclQualifierPtr * q_in)
{
  if (q_in) {
    dclQualifierPtr q = *q_in;
    if (q) {
      int i;
      if (q->name)
	free(q->name);
      if (q->defaultValue)
	free(q->defaultValue);
      if (q->type)
	free(q->type);
      if (q->syntax)
	free(q->syntax);
      if (q->values) {
	for (i = 0; i < q->value_count; i++)
	  free(q->values[i]);
	free(q->values);
      }
      free(q);
      *q_in = 0;
    }
  }
}

/*! Free all parameters and qualifiers from command structure.
 \param cmdDef [in] A pointer to a dclCommand structure.
*/

void freeCommandParamsAndQuals(dclCommandPtr cmdDef)
{
  if (cmdDef) {
    int i;
    if (cmdDef->parameter_count > 0) {
      for (i = 0; i < cmdDef->parameter_count; i++)
	freeParameter(&cmdDef->parameters[i]);
      cmdDef->parameter_count = 0;
      free(cmdDef->parameters);
      cmdDef->parameters = 0;
    }
    if (cmdDef->qualifier_count > 0) {
      for (i = 0; i < cmdDef->qualifier_count; i++)
	freeQualifier(&cmdDef->qualifiers[i]);
      cmdDef->qualifier_count = 0;
      free(cmdDef->qualifiers);
      cmdDef->qualifiers = 0;
    }
  }
}

/*! Free memory associated with a command structure.
 \param cmd [in,out] The address of a pointer to a dclCommand structure
*/

static void freeCommand(dclCommandPtr * cmd_in)
{
  if (cmd_in) {
    dclCommandPtr cmd = *cmd_in;
    if (cmd) {
      if (cmd->verb)
	free(cmd->verb);
      if (cmd->routine)
	free(cmd->routine);
      if (cmd->command_line)
	free(cmd->command_line);
      freeCommandParamsAndQuals(cmd);
      free(cmd);
      *cmd_in = 0;
    }
  }
}

/*! Find the information associated with a command verb.
  - Locate all parameters and qualifiers defined for the command
  and load the associated information included for those params
  and qualifiers.
  - Locate a handler routine for the command.

  ** NOTE - findVerbInfo will recurse to find the command parts. **

 \param node [in] A pointer to a child node of a command verb node
                  found in the xml document.
 \param cmd [in] A pointer to a dclCommand structure.

*/

static void findVerbInfo(xmlNodePtr node, dclCommandPtr cmd)
{

  /* If the parent node is a verb */

  xmlNodePtr parent = node->parent;
  if (parent->name && (strcasecmp(parent->name, "verb") == 0)) {
    struct _xmlAttr *propNode;

    /* check to see if type is rest_of_line.
       If it is just set the cmd->rest_of_line and return;
     */

    for (propNode = parent->properties; propNode; propNode = propNode->next) {
      if (propNode->name &&
	  (strcasecmp(propNode->name, "type") == 0) &&
	  propNode->children && propNode->children->content &&
	  (strcasecmp(propNode->children->content, "rest_of_line") == 0)) {
	cmd->rest_of_line = 1;
	break;
      }
    }
  }

  /* If the node ia a paramter (i.e. <parameter ... />) */

  if ((cmd->rest_of_line == 0) && node->name && (strcasecmp(node->name, "parameter") == 0)) {
    struct _xmlAttr *propNode;

    /* allocate an empty parameter structure */

    dclParameterPtr parameter = memset(malloc(sizeof(dclParameter)), 0, sizeof(dclParameter));

    /* Look at all the properties of the parameter */

    for (propNode = node->properties; propNode; propNode = propNode->next) {

      /* If this is a label property (i.e. <parameter label=gub ... />)
         duplicate the label value into the dclParameter structure. */

      if (propNode->name &&
	  (strcasecmp(propNode->name, "label") == 0) &&
	  propNode->children && propNode->children->content) {
	parameter->label = strdup(propNode->children->content);

	/* else if this is a name property duplicate the name value */

      } else if (propNode->name &&
		 (strcasecmp(propNode->name, "name") == 0) &&
		 propNode->children && propNode->children->content) {
	parameter->name = strdup(propNode->children->content);

	/* else if this is a required property set required
	   if value is True. */

      } else if (propNode->name &&
		 (strcasecmp(propNode->name, "required") == 0) &&
		 propNode->children && propNode->children->content) {
	parameter->required = strcasecmp(propNode->children->content, "True") == 0;

	/* else if this is a type property duplicate the type value
	   unless the type is "REST_OF_LINE" which signifies the parameter
	   should consume the rest of the line. In the rest of line case
	   the useRestOfLine flag is set.

	   ** Note this indicates an alternate command definition based
	   on the value specified by the user for this parameter.
	   The cmd definition may be completely replaced later when
	   user input is analyzed. ** */

      } else if (propNode->name &&
		 (strcasecmp(propNode->name, "type") == 0) &&
		 propNode->children && propNode->children->content) {
	if (strcasecmp(propNode->children->content, "rest_of_line") == 0) {
	  parameter->useRestOfLine = 1;
	} else {
	  parameter->type = strdup(propNode->children->content);
	}

	/* else if this ia a prompt property duplicate into the parameter
	   structure. The prompt may be sent to the user if he failed
	   to provide this parameter in the command string. */

      } else if (propNode->name &&
		 (strcasecmp(propNode->name, "prompt") == 0) &&
		 propNode->children && propNode->children->content) {
	parameter->prompt = strdup(propNode->children->content);

	/* else if this ia a list property set listOk in the parameter
	   if value is "True". When processing a command if the user
	   provided multiple values for the parameter a syntax error
	   will be issued. */
      } else if (propNode->name &&
		 (strcasecmp(propNode->name, "list") == 0) &&
		 propNode->children &&
		 propNode->children->content &&
		 (strcasecmp(propNode->children->content, "true") == 0)) {
	parameter->listOk = 1;
      } else if (propNode->name &&
		 (strcasecmp(propNode->name, "default") == 0) &&
		 propNode->children && propNode->children->content) {
	char *value = strdup(propNode->children->content);
	char *c,*v;
	for (v = value,c = strchr(v, ','); strlen(v); c = strchr(v, ',')) {
	  if (c)
	    *c = 0;
	  if (parameter->values) {
	    parameter->values =
		realloc(parameter->values, (parameter->value_count + 1) * sizeof(char *));
	  } else {
	    parameter->values = malloc(sizeof(char *));
	  }
	  parameter->values[parameter->value_count++] = strdup(v);
	  if (c)
	    v = c+1;
	  else
	    break;
	}
	free(value);
      }
    }

    /* Add this parameter to the dclCommand struture. */

    if (cmd->parameter_count == 0)
      cmd->parameters = malloc(sizeof(dclParameterPtr));
    else
      cmd->parameters =
	  realloc(cmd->parameters, sizeof(dclParameterPtr) * (cmd->parameter_count + 1));
    cmd->parameters[cmd->parameter_count] = parameter;
    cmd->parameter_count++;

    /* else if this is a qualifier node (i.e. <qualifier ... />) */

  } else if ((cmd->rest_of_line == 0) && node->name && (strcasecmp(node->name, "qualifier") == 0)) {

    /* allocate an empty dclQualifier structure. */

    dclQualifierPtr qualifier = memset(malloc(sizeof(dclQualifier)), 0, sizeof(dclQualifier));
    struct _xmlAttr *propNode;

    /* examine all the properties of this node */

    for (propNode = node->properties; propNode; propNode = propNode->next) {

      /* if this is a name property, duplicate to the qualifier structure. */
      if (propNode->name &&
	  (strcasecmp(propNode->name, "name") == 0) &&
	  propNode->children && propNode->children->content) {
	qualifier->name = strdup(propNode->children->content);

	/* else if this is a defaulted property, set the isDefault
	   flag if value is "True". If isDefault is true when
	   processing the command, this qualifier will be included unless
	   preceded by "no" */

      } else if (propNode->name &&
		 (strcasecmp(propNode->name, "defaulted") == 0) &&
		 propNode->children && propNode->children->content) {
	qualifier->isDefault = strcasecmp(propNode->children->content, "True") == 0;

	/* else if this is a required property, set the valueRequired flag
	   if the value is "True". If valueRequired is true when
	   processing the command, this will check to make sure the user
	   provided a value with the qualifier (i.e. /qualifier=value).
	 */

      } else if (propNode->name &&
		 (strcasecmp(propNode->name, "required") == 0) &&
		 propNode->children && propNode->children->content) {
	qualifier->valueRequired = strcasecmp(propNode->children->content, "True") == 0;

	/* else if this is a nonnegatable property, set the nonnegateable flag
	   if the value is "True". If nonnegatable is set then the user cannot
	   prefix the qualifier with "no" (i.e. cmd /nogub is disallowed). */

      } else if (propNode->name &&
		 (strcasecmp(propNode->name, "nonnegatable") == 0) &&
		 propNode->children && propNode->children->content) {
	qualifier->nonnegatable = strcasecmp(propNode->children->content, "True") == 0;

	/* else if this is a list property, set the listOk flag if the value
	   is "True". The listOk flag indicates whether more than one value
	   is permitted with the qualifier. (i.e. cmd /gub=(val1,val2,val3)) */

      } else if (propNode->name &&
		 (strcasecmp(propNode->name, "list") == 0) &&
		 propNode->children && propNode->children->content) {
	qualifier->listOk = strcasecmp(propNode->children->content, "True") == 0;

	/* else if this ia a type property, duplicate the type value in the
	   qualifier struct. The type property is used when processing the qualifier
	   to check for valid values or whether the value(s) should be converted
	   to integer during cli queries. */

      } else if (propNode->name &&
		 (strcasecmp(propNode->name, "type") == 0) &&
		 propNode->children && propNode->children->content) {
	qualifier->type = strdup(propNode->children->content);

	/* else if this is a default property duplicate the value into
	   the defaultValue qualifier struct member. This will be used
	   to specify a default value if the user did not specify a value
	   with the qualifier */

      } else if (propNode->name &&
		 (strcasecmp(propNode->name, "default") == 0) &&
		 propNode->children && propNode->children->content) {
	qualifier->defaultValue = strdup(propNode->children->content);

	/* else if this is a syntax property duplicate the syntax name
	   into the qualifier structure. This will cause a command syntax
	   replacement during command processing if the user included
	   this qualifier in the command. */

      } else if (propNode->name &&
		 (strcasecmp(propNode->name, "syntax") == 0) &&
		 propNode->children && propNode->children->content) {
	qualifier->syntax = strdup(propNode->children->content);
      }
    }
    /* Add the qualifier to the command definition */
    if (cmd->qualifier_count == 0)
      cmd->qualifiers = malloc(sizeof(dclQualifierPtr));
    else
      cmd->qualifiers =
	  realloc(cmd->qualifiers, sizeof(dclQualifierPtr) * (cmd->qualifier_count + 1));
    cmd->qualifiers[cmd->qualifier_count] = qualifier;
    cmd->qualifier_count++;

    /* else if this is a routine node (i.e. <routine ... /> 
       duplicate the routine name into the command routine element. */

  } else if (node->name && (strcasecmp(node->name, "routine") == 0)) {
    if (node->properties && node->properties->children && node->properties->children->content) {
      if (cmd->routine)
	free(cmd->routine);
      cmd->routine = strdup(node->properties->children->content);
    }
  }

  /* if this node has a next sibling, process it next */

  if (node->next != NULL)
    findVerbInfo(node->next, cmd);
}

/*| Find all nodes in the xml hierarchy with an xml "name" matching the category argument with it's content matching
   the name argument stopping if there is an exact match. The matching is case insensitive and only checks the number
   of characters provided by the name parameter. If their is a match and the node property has exacly the same
   length as the name parameter then it is considered as an exact match.

   \param node [in] Node to begin search with.
   \param category [in] XML Node name to search for (i.e. "verb" would look for xml <verb .../> items).
   \param name [in] Value to match with the content of the first property of the node found based on category.
   \param list [in] Pointer to a dclNodList which describes an array of xmlNodes
   \param exactFound [in,out] Pointer to an int flag which is set if the name parameter exactly matches the
          property of the xml node.

   *** NOTE: This is only applicable for use on xml nodes which look like <category name="name"/> ***
   *** NOTE: This routine recurses on node siblings and children. ****
   */

static void findEntity(xmlNodePtr node, char *category, char *name, dclNodeListPtr list,
		       int *exactFound)
{

  /* If exact match already found just return */

  if (*exactFound)
    return;

  /* else if all the characters in the name argument match the property content consider it a match. */

  else if (node->name &&
	   (strcasecmp(node->name, category) == 0) &&
	   node->properties &&
	   node->properties->children &&
	   node->properties->children->content &&
	   (strncasecmp(name, node->properties->children->content, strlen(name)) == 0)) {

    /* Check if it is an exact match */
    if ((name == NULL) || (strlen(name) == strlen(node->properties->children->content))) {	// if exact command match use it!

      /* if already found other nodes but not exact match then free the "array" of nodes. */

      if (list->count > 1)
	free(list->nodes);

      /* allocate memory for one node ptr and load this node and return */
      list->nodes = malloc(sizeof(xmlNodePtr));
      list->nodes[0] = node;
      list->count = 1;
      *exactFound = 1;
      return;
    }

    /* Add this node to the list of nodes */

    if (list->count == 0) {
      list->nodes = malloc(sizeof(xmlNodePtr));
    } else {
      list->nodes = realloc(list->nodes, sizeof(xmlNodePtr) * (list->count + 1));
    }
    list->nodes[list->count] = node;
    list->count++;
  }

  /* If this node has a sibling see if it matches. */

  if (node->next)
    findEntity(node->next, category, name, list, exactFound);

  /* If this node has children see if they match. */

  if (node->children)
    findEntity(node->children, category, name, list, exactFound);
}

/*! Invoke the handler routine passing a command definition context,
  a place to store an error message if any and a place to store output
  if any.

  The cmd line structure undergoes final processing by merging in default
  values, merging in the xml command definition properties found for the command,
  and replace the names of the qualifier and parameters with their full names
  from the xml command definition.

  \param image [in] The shared library image name which should contain the handler routine.
  \param cmd [in] The command definition constructed from the user input.
  \param cmdDef [in] The command definition constructed from the xml command defintion.
  \param prompt [out] The address of a pointer to a string where a prompt string will be
                      written if the command processing requests for additional information.
                      This pointer must be freed by the callers if not NULL.
  \param error [out] The address of a pointer to an error message.
                     This pointer must be freed by the callers if not NULL.
  \param output [out] The address of a pointer to command output text.
                      This pointer must be freed by the callers if not NULL.
*/

static int dispatchToHandler(char *image, dclCommandPtr cmd, dclCommandPtr cmdDef, char **prompt,
			     char **error, char **output)
{
  int i;
  int (*handler) (dclCommandPtr, char **, char **);
  int status;

  /* Make sure the command processing discovered the name of the handler routine.
   *** NOTE: This should not happen and indicates a problem with the command table xml definition! ***
   */

  if (cmdDef->rest_of_line)
    goto rest_of_line;

  /* Check to see if a parameter is designated as "useRestOfLine' and replace the value with the
     complete text of the rest of the command line. Throw away the trailing parameters and qualifiers if any. */

  for (i = 0; i < cmdDef->parameter_count; i++) {
    if (cmdDef->parameters[i]->useRestOfLine) {
      int j, nq = 0;

      /* If user included this parameter */
      if (cmd->parameter_count > i) {

	/* save the rest of line */
	char *rol = cmd->parameters[i]->restOfLine;

	/* clear the rest of line from the parameter so it doesn't get freed twice */
	cmd->parameters[i]->restOfLine = 0;

	/* Free any trailing parameters */
	for (j = i + 1; j < cmd->parameter_count; j++)
	  freeParameter(&cmd->parameters[j]);

	/* Reset the parameter count for the command */
	cmd->parameter_count = i + 1;

	/* Use the rest of line for the value of the parameter freeing any other values stored during parsing. */
	for (j = 0; j < cmd->parameters[i]->value_count; j++)
	  free(cmd->parameters[i]->values[j]);
	cmd->parameters[i]->values[0] = rol;
	cmd->parameters[i]->value_count = 1;
      }
      for (j = cmd->qualifier_count; j > 0; j--) {
	if (cmd->qualifiers[j - 1]->position > i) {
	  freeQualifier(&cmd->qualifiers[j - 1]);
	  cmd->qualifier_count--;
	  if (cmd->qualifier_count == 0) {
	    free(cmd->qualifiers);
	    cmd->qualifiers = 0;
	  }
	}
      }
      break;
    }
  }

  /* Check to make sure the qualifiers specified are value for this command */

  for (i = 0; i < cmd->qualifier_count; i++) {
    int j;
    for (j = 0; j < cmdDef->qualifier_count; j++) {
      if (strncasecmp(cmd->qualifiers[i]->name, cmdDef->qualifiers[j]->name,
		      strlen(cmd->qualifiers[i]->name)) == 0) {
	break;
      } else if ((strncasecmp(cmd->qualifiers[i]->name, "no", 2) == 0) &&
		 (strlen(cmd->qualifiers[i]->name) > 2) &&
		 (strncasecmp(cmd->qualifiers[i]->name + 2,
			      cmdDef->qualifiers[j]->name,
			      strlen(cmd->qualifiers[i]->name) - 2) == 0)) {
	break;
      }
    }
    if (j == cmdDef->qualifier_count) {
      char *errstr = malloc(100);
      sprintf(errstr, "Qualifier \"%s\" is not valid for this command\n", cmd->qualifiers[i]->name);
      *error = errstr;
      return CLI_STS_IVQUAL;
    }
  }
  /* Check to make sure there are not too many parameters */

  if (cmd->parameter_count > cmdDef->parameter_count) {
    char *errstr = malloc(100);
    sprintf(errstr,
	    "Too many parameters specified in the command. Maximum supported is %d. Provided was %d.\n",
	    cmdDef->parameter_count, cmd->parameter_count);
    *error = errstr;
    return CLI_STS_TOO_MANY_PRMS;
  }

  /* For all the parameters */

  for (i = 0; i < cmdDef->parameter_count; i++) {

    /* Check to make sure user did not specify too many values in parameters. */

    if (cmdDef->parameters[i]->listOk == 0) {
      if ((i < cmd->parameter_count) && (cmd->parameters[i]->value_count > 1)) {
	char *errstr = malloc(100);
	sprintf(errstr, "Parameter number %d does not accept a list of values. "
		"Perhaps that parameter needs to enclosed in double quotes?\n", i + 1);
	*error = errstr;
	return CLI_STS_TOO_MANY_VALS;
      }
    }

    /* Check to see if the parameter has a default value and the parameter was not provided */

    if ((cmdDef->parameters[i]->value_count > 0) && ((i + 1) > cmd->parameter_count)) {
      int j;
      dclParameterPtr pdef = cmdDef->parameters[i];
      dclParameterPtr p = memset(malloc(sizeof(dclParameter)), 0, sizeof(dclParameter));
      p->name = strdup(pdef->name);
      if (pdef->label)
	p->label = strdup(pdef->label);
      p->value_count = pdef->value_count;
      p->values = malloc(p->value_count * sizeof(char *));
      for (j = 0; j < p->value_count; j++)
	p->values[j] = strdup(pdef->values[j]);
      if (cmd->parameter_count > 0) {
	cmd->parameters = realloc(cmd->parameters, (cmd->parameter_count + 1) * sizeof(char *));
      } else {
	cmd->parameters = malloc(sizeof(char *));
      }
      cmd->parameters[cmd->parameter_count] = p;
      cmd->parameter_count++;
    }

    /* Check to see if a required parameter was not specified. */

    if (cmdDef->parameters[i]->required) {
      if (cmd->parameter_count < (i + 1)) {
	*prompt = strdup(cmdDef->parameters[i]->prompt ? cmdDef->parameters[i]->prompt : "What");
	return CLI_STS_PROMPT_MORE;
      }
    }
  }

  /* For each possible qualifier */

  for (i = 0; i < cmdDef->qualifier_count; i++) {
    int j;

    /* If the qualifier is required and was used in the command, make sure user provided a value */
    if (cmdDef->qualifiers[i]->valueRequired != 0) {
      int q;
      for (q = 0; q < cmd->qualifier_count; q++) {
	if (strncasecmp
	    (cmd->qualifiers[q]->name, cmdDef->qualifiers[i]->name,
	     strlen(cmd->qualifiers[q]->name)) == 0) {
	  if (cmd->qualifiers[q]->value_count == 0) {
	    char *errstr = malloc(100);
	    sprintf(errstr, "Qualifier \"%s\" requires a value and none was provided\n",
		    cmdDef->qualifiers[i]->name);
	    *error = errstr;
	    return CLI_STS_MISSING_VALUE;
	  }
	  break;
	}
      }
    }

    /* Look for qualifiers that are default qualifiers and add them if not already there in cmd */
    if (cmdDef->qualifiers[i]->isDefault) {

      /* Look to see if the default qualifier is already provided by user. */

      for (j = 0; j < cmd->qualifier_count; j++) {
	if (strncasecmp
	    (cmd->qualifiers[j]->name, cmdDef->qualifiers[i]->name,
	     strlen(cmd->qualifiers[j]->name)) == 0) {
	  break;
	}
      }

      /* If not already provided add this qualifier with default value if any */

      if (j == cmd->qualifier_count) {
	dclQualifierPtr qualifier = memset(malloc(sizeof(dclQualifier)), 0, sizeof(dclQualifier));
	qualifier->name = strdup(cmdDef->qualifiers[i]->name);
	if (cmdDef->qualifiers[i]->defaultValue != NULL) {
	  qualifier->value_count = 1;
	  qualifier->values = malloc(sizeof(char *));
	  qualifier->values[0] = strdup(cmdDef->qualifiers[i]->defaultValue);
	}
	if (cmd->qualifier_count == 0) {
	  cmd->qualifiers = malloc(sizeof(dclQualifierPtr));
	} else {
	  cmd->qualifiers =
	      realloc(cmd->qualifiers, sizeof(dclQualifierPtr) * (cmd->qualifier_count + 1));
	}
	cmd->qualifiers[cmd->qualifier_count] = qualifier;
	cmd->qualifier_count++;
      }
    }
  }

  /* Replace any parameter names and labels with the real command definition parameter names. */

  for (i = 0; i < cmd->parameter_count; i++) {
    if (cmd->parameters[i]->name)
      free(cmd->parameters[i]->name);
    cmd->parameters[i]->name = strdup(cmdDef->parameters[i]->name);
    if (cmd->parameters[i]->label)
      free(cmd->parameters[i]->label);
    cmd->parameters[i]->label =
	(cmdDef->parameters[i]->label) ? strdup(cmdDef->parameters[i]->label) : 0;
  }

  /* For all the qualifiers specified */

  for (i = 0; i < cmd->qualifier_count; i++) {
    int q;

    /* for all possible qualifiers */

    for (q = 0; q < cmdDef->qualifier_count; q++) {
      char *realname = cmdDef->qualifiers[q]->name;
      char *negated = strcpy(malloc(strlen(realname) + 3), "no");
      strcat(negated, realname);
      if ((strncasecmp(cmd->qualifiers[i]->name, realname, strlen(cmd->qualifiers[i]->name)) == 0)
	  || (strncasecmp(cmd->qualifiers[i]->name, negated, strlen(cmd->qualifiers[i]->name)) ==
	      0)) {

	/************* process type ************/
	/* Check if there are too many values specified */

	if ((cmd->qualifiers[i]->value_count > 1) && (cmdDef->qualifiers[q]->listOk == 0)) {
	  char *errstr = malloc(100);
	  sprintf(errstr, "Qualifier \"%s\" does not permit a list of values\n",
		  cmdDef->qualifiers[i]->name);
	  *error = errstr;
	  return CLI_STS_TOO_MANY_VALS;
	}

	/* Check to see if the command was negated and if that is allowed */

	if ((strncasecmp(cmd->qualifiers[i]->name, "no", 2) == 0) &&
	    (strncasecmp(realname, "no", 2) != 0)) {
	  if (cmdDef->qualifiers[q]->nonnegatable) {
	    char *errstr = malloc(100);
	    sprintf(errstr, "Qualifier \"%s\" cannot be negated\n", realname);
	    *error = errstr;
	    return CLI_STS_NOTNEGATABLE;
	  } else
	    cmd->qualifiers[i]->negated = 1;
	}

	/* set the qualifier name to the realname */

	if (cmd->qualifiers[i]->name)
	  free(cmd->qualifiers[i]->name);
	cmd->qualifiers[i]->name = strdup(realname);
	free(negated);
	break;
      }
      free(negated);
    }
    if (q == cmdDef->qualifier_count) {
      char *errstr = malloc(100);
      sprintf(errstr, "Qualifier \"%s\" is not valid for this command\n", cmd->qualifiers[i]->name);
      *error = errstr;
      return CLI_STS_IVQUAL;
    }
  }
 rest_of_line:
  if (cmdDef->routine == NULL) {
    *error =
	strdup
	("No execution routine specified in command definition. Internal error, please report to MDSplus developers\n");
    fprintf(stderr, "Command not supported\n");
    return CLI_STS_IVVERB;
  } else {
    if (cmd->routine)
      free(cmd->routine);
    cmd->routine = strdup(cmdDef->routine);
  }

  if (strcmp(image, "mdsdcl_commands") == 0)
    image = "Mdsdcl";
  status = LibFindImageSymbol_C(image, cmdDef->routine, &handler);
  if (status & 1) {
    status = handler(cmd, error, output);
  }
  return status;
}

/*! Process the command provided by the user to find the closest defined command
    based on the verb of the command. Then see if the parameters or qualifiers defined
    for the command can trigger a new command syntax. If so, reprocess the command
    based on the new syntax. Once the final syntax is determined call the
    dispatchToHandler to perform final command preprocessing and then call the
    command execution handler routine. This is done by the following steps:

      1) Get the command information which matches the verb specified in the command.
      2) For each possible parameter permitted by the verb.
           Check to see if the parameter is used to specify a new command syntax.
           This is common for verbs like "SET" where the next parameter changes
	   the type of command being issued (i.e. SET TREE, SET DEFAULT,...))
	   If a new syntax is controlled by this parameter reload the defined command
	   information based on this new syntax and restart the processing based
	   on this new command definition.
      3) Similarly, for each qualifier provided check the command definition to
         see if any command alters the syntax of the command and if so reload the
	 defined command info base on this syntax switch. An example of this
	 is the "DIRECTORY /TAG" command which uses a different handler than
	 the "DIRECTORY" command without that qualifier.
      4) Unless there were errors (i.e. no matching verb or matching parameter
         where a new syntax is expected, call the dispatchTohandler routine with
	 the following arguments:
	 - The command table name where the command definition was found. (Used
	   to identify the library where the execution handlers should be found.)
         - The parsed command definition provided by the user.
         - The matching command definition from the command definition tables.
	 - A pointer to a prompt string in case the command needs to prompt for
	   more input.
	 - A pointer to an error string where any specific error information can
	   be returned.
	 - A pointer to an output string.
   
  \param docList [in] The list of loaded command tables.
  \param verbNode [in] The node of the matching verb in the command xml table.
  \param cmd [in] The parsed user command definition.
  \param cmdDef [in] The command definition information from the command table.
  \param prompt [out] A prompt string if more information is needed. Must be freed if not NULL.
  \param error [out] An error string if errors detected. Must be freed if not NULL.
  \param output [out] The output string if any. Must be freed if not NULL.
*/

int processCommand(dclDocListPtr docList, xmlNodePtr verbNode_in, dclCommandPtr cmd,
		   dclCommandPtr cmdDef, char **prompt, char **error, char **output)
{
  xmlDocPtr doc = docList->doc;
  int i;
  int status = 0;
  int redo = 1;
  int isSyntax = 0;
  xmlNodePtr verbNode = verbNode_in;

  /* loop in case syntax changes occur based on parameter or qualifiers */

  while (redo) {
    redo = 0;

    /* Gather all the known information about the command being processed from the xml command description document */

    findVerbInfo(verbNode->children, cmdDef);

    /* For each possible parameter */
    for (i = 0; ((!isSyntax) && (i < cmdDef->parameter_count)); i++) {

      /* if parameter has a type */

      if ((!isSyntax) && (cmdDef->parameters[i]->type != NULL)) {
	xmlNodePtr syntaxNode = 0;

	/* If parameter was used in the command look up a type definition based on the value of the parameter specified */
	if (cmd->parameter_count > i && cmd->parameters[i]->value_count == 1) {
	  char *keywordName = cmd->parameters[i]->values[0];
	  int exactFound = 0;
	  dclNodeList list;
	  memset(&list, 0, sizeof(dclNodeList));
	  findEntity(doc->children, "type", cmdDef->parameters[i]->type, &list, &exactFound);

	  /* If only one match found (which should be the case unless the command definition is invalid!) */

	  if (list.count == 1) {
	    xmlNodePtr typeNode = list.nodes[0];
	    xmlNodePtr keywordNode;
	    free(list.nodes);

	    /* For all the keywords of the type see if the parameter value matches it */

	    for (keywordNode = typeNode->children; keywordNode; keywordNode = keywordNode->next) {
	      xmlNodePtr mNode = 0;
	      if (keywordNode->name && (strcasecmp(keywordNode->name, "keyword") == 0)) {
		struct _xmlAttr *nameNode = keywordNode->properties;
		if (nameNode && nameNode->name && (strcasecmp(nameNode->name, "name") == 0)) {
		  if (nameNode->children &&
		      nameNode->children->content &&
		      (strncasecmp(keywordName, nameNode->children->content, strlen(keywordName)) ==
		       0)) {
		    struct _xmlAttr *syntaxNode = nameNode->next;
		    if (syntaxNode && syntaxNode->children && syntaxNode->children->content) {
		      dclNodeList list = { 0, 0 };
		      int exactFound = 0;

		      /* Find the syntax specified in the keyword */

		      findEntity(doc->children, "syntax", syntaxNode->children->content, &list,
				 &exactFound);

		      /* If found (which should be the case unless error in the command definition) reprocess the cmd
		         using the new syntax definition */

		      if (list.count == 1) {
			redo = 1;
			isSyntax = 1;
			verbNode = list.nodes[0];
			findVerbInfo(((xmlNodePtr) (list.nodes[0]))->children, cmdDef);
		      }
		      if (list.nodes)
			free(list.nodes);
		      goto REDO;
		    }
		  }
		}
	      }
	    }

	    /* If no keywords match the parameter value then get out. It's an invalid command */
	    status = CLI_STS_IVVERB;
	    goto DONE;
	  }
	}
      }
    }

    /* process syntax switching on qualifiers */

    for (i = 0; (redo == 0) && (i < cmdDef->qualifier_count); i++) {
      if (cmdDef->qualifiers[i]->syntax != NULL) {
	int q;
	for (q = 0; q < cmd->qualifier_count; q++) {
	  char *realname = cmdDef->qualifiers[i]->name;
	  char *negated = strcpy(malloc(strlen(realname) + 3), "no");
	  strcat(negated, realname);
	  if ((strncasecmp(cmd->qualifiers[q]->name, realname, strlen(cmd->qualifiers[q]->name)) ==
	       0)
	      || (strncasecmp(cmd->qualifiers[q]->name, negated, strlen(cmd->qualifiers[q]->name))
		  == 0)) {
	    dclNodeList list = { 0, 0 };
	    int exactFound = 0;
	    findEntity(doc->children, "syntax", cmdDef->qualifiers[i]->syntax, &list, &exactFound);
	    if (list.count == 1) {
	      int k;
	      freeQualifier(&cmd->qualifiers[q]);
	      cmd->qualifier_count--;
	      for (k = q; k < cmd->qualifier_count; k++)
		cmd->qualifiers[k] = cmd->qualifiers[k + 1];
	      if (cmd->qualifier_count == 0) {
		free(cmd->qualifiers);
		cmd->qualifiers = 0;
	      }
	      isSyntax = 1;
	      redo = 1;
	      verbNode = list.nodes[0];
	    }
	    if (list.nodes)
	      free(list.nodes);
	    free(negated);
	    goto REDO;
	  }
	  free(negated);
	}
      }
    }

 REDO:
    if (redo) {
      freeCommandParamsAndQuals(cmdDef);
    }
  }
  if (status == 0) {
    status = dispatchToHandler(docList->name, cmd, cmdDef, prompt, error, output);
  }
 DONE:
  return status;
}

/*! Set the prompt and def_file if defined in the command table xml. These
  should be properties of the top module tag, for example:

     <module name="tcl_commands" prompt="TCL> " def_file=".tcl">


  \param doc [in] The xml document pointer.
*/

static void mdsdclSetupCommands(xmlDocPtr doc) {
  /* Set prompt and def_file if defined in top level module key */
    
  struct _xmlAttr *p;
  if (doc->children && doc->children) {
    for (p=doc->children->properties; p; p=p->next) {
      if ((strcasecmp(p->name,"prompt") == 0) &&
	  p->children &&
	  p->children->content)
	mdsdclSetPrompt(p->children->content);
      else if ((strcasecmp(p->name,"def_file") == 0) &&
	       p->children &&
	       p->children->content)
	mdsdclSetDefFile(p->children->content);
    }
  }
}

/*! Add a command table by parsing an xml command definition file.
    The file is located in a directory specified by an environment
    variable "MDSXML" or the current directory if that environment
    variable is not defined.

  \param name [in] The name of the command table (i.e tcl or tcl_commands)
  \param error [out] An error message if trouble finding and/or parsing
                     the xml command definition file.
*/

int mdsdclAddCommands(char *name_in, char **error)
{
  int i;
  char *name = 0;
  char *commands;
  char *commands_part;
  xmlDocPtr doc;
  dclDocListPtr doc_l, doc_p;
  char *mdsplus_dir;
  char *filename = 0;
  int status = 0;
  int loaded = 0;
  name = strdup(name_in);

  /* convert the name to lowercase. The table xml files should always be named
     as tablename_commands.xml all lowercase. */

  for (i = 0; i < strlen(name); i++)
    name[i] = tolower(name[i]);

  /* see if the caller included the "_commands" suffix in the name and if not
     add it */

  commands_part = strstr(name, "_commands");
  if (commands_part && ((name + strlen(name_in) - strlen("_commands")) == commands_part))
    commands_part[0] = '\0';
  commands = strcpy(malloc(strlen(name) + strlen("_commands") + 1), name);
  strcat(commands, "_commands");
  free(name);

  /* See if that command table has already been loaded. If it has, pop that table
     to the top of the stack and return */

  for (doc_l = dclDocs, doc_p = 0; doc_l; doc_p = doc_l, doc_l = doc_l->next) {
    if (strcmp(doc_l->name, commands) == 0) {
      if (doc_p) {
	doc_p->next = doc_l->next;
	doc_l->next = dclDocs;
	dclDocs = doc_l;
      }
      free(commands);
      mdsdclSetupCommands(dclDocs->doc);
      return 0;
    }
  }

  /* Initialize the xml parser */

  xmlInitParser();

  /* Look for command definitions in $MDSPLUS_DIR/xml/ */

  mdsplus_dir = getenv("MDSPLUS_DIR");
  if (mdsplus_dir == 0)
    mdsplus_dir = strdup(".");
  else {
    mdsplus_dir = strcpy(malloc(strlen(mdsplus_dir) + 10), mdsplus_dir);
    strcat(mdsplus_dir, "/xml");
  }

  filename = strcpy(malloc(strlen(commands) + strlen(mdsplus_dir) + 10), mdsplus_dir);
  strcat(filename, "/");
  strcat(filename, commands);
  strcat(filename, ".xml");
  free(mdsplus_dir);
  /* Try parsing the xml file if it exists */

  doc = xmlParseFile(filename);

  /* If cannot find the file or parse it, set the error string */

  if (doc == 0) {
    char *errstr = malloc(strlen(filename) + 50);
    sprintf(errstr, " Error: unable to parse %s\n", filename);
    *error = errstr;
    status = -1;
  } else {


    /* else stick the parsed xml document at the top of the command stack */

    doc_l = malloc(sizeof(dclDocList));
    doc_l->name = commands;
    doc_l->doc = doc;
    doc_l->next = dclDocs;
    dclDocs = doc_l;
    status = 0;
    mdsdclSetupCommands(dclDocs->doc);
  }
  free(filename);
  return status;
}

int mdsdcl_do_command(char const *command)
{
  return mdsdcl_do_command_extra_args(command, 0, 0, 0);
}

int cmdExecute(dclCommandPtr cmd, char **prompt_out, char **output_out, char **error_out)
{
  int status = CLI_STS_IVVERB;
  char *prompt = 0;
  char *error_tmp = 0;
  char *error = 0;
  char *output = 0;
  int cli_status = CLI_STS(0);
  dclDocListPtr doc_l;
  if (dclDocs == NULL)
    mdsdclAddCommands("mdsdcl_commands", &error);
  for (doc_l = dclDocs;
       ((status & 0xffff0000) == cli_status) && (doc_l != NULL) && status != CLI_STS_PROMPT_MORE;
       doc_l = doc_l->next) {
    dclCommandPtr cmdDef = memset(malloc(sizeof(dclCommand)), 0, sizeof(dclCommand));
    cmdDef->verb = strdup(cmd->verb);
    int exactFound = 0;
    dclNodeList matchingVerbs = { 0, 0 };
    findEntity(((xmlDocPtr) (doc_l->doc))->children, "verb", cmdDef->verb, &matchingVerbs,
	       &exactFound);
    if (matchingVerbs.count == 0 || matchingVerbs.count > 1) {
      if (matchingVerbs.nodes != NULL)
	free(matchingVerbs.nodes);
      status = CLI_STS_IVVERB;
    } else {
      status =
	  processCommand(doc_l, matchingVerbs.nodes[0], cmd, cmdDef, &prompt, &error_tmp, &output);
      if (status & 1) {
	if (error)
	  free(error);
	error = error_tmp;
      } else {
	if (error_tmp) {
	  if (error == NULL)
	    error = error_tmp;
	  else
	    free(error_tmp);
	}
      }
      free(matchingVerbs.nodes);
      if (status == 0) {
	freeCommand(&cmdDef);
	break;
      }
    }
    freeCommand(&cmdDef);
  }
  freeCommand(&cmd);
  if (status == CLI_STS_PROMPT_MORE) {
    if (!prompt) {
      prompt = strdup("What: ");
    }
  }
  if (prompt != NULL) {
    if (prompt_out)
      *prompt_out = prompt;
    else {
      if (error != NULL) {
	error = strcat(realloc(error, strlen(error) + 100),
		       "\nCommand incomplete, missing parameter or qualifier value.");
      }
      free(prompt);
    }
  }
  if ((prompt == NULL) && (error == 0) && (!(status & 1))) {
    if (status == CLI_STS_IVVERB && error == NULL) {
      error = strcpy(malloc(100), "mdsdcl: No such command\n");
    } else {
      char *msg = MdsGetMsg(status);
      error = malloc(strlen(msg) + 10);
      sprintf(error, "Error: %s\n", msg);
    }
  }
  if (error != NULL) {
    if (error_out == NULL) {
      fprintf(stderr, "%s", error);
      free(error);
    } else
      *error_out = error;
  }
  if (output != NULL) {
    if (output_out == NULL) {
      fprintf(stdout, output);
      free(output);
    } else {
      if (*output_out) {
	*output_out = strcat(realloc(*output_out,strlen(*output_out)+strlen(output)+1),output);
	free(output);
      }
      else
	*output_out = output;
      mdsdclFlushOutput(*output_out);
    }
  }
  return status;
}

int cli_present(void *ctx, char *name)
{
  dclCommandPtr cmd = (dclCommandPtr) ctx;
  int i;
  int ans = CLI_STS_ABSENT;
  for (i = 0; (ans == CLI_STS_ABSENT) && (i < cmd->parameter_count); i++) {
    char *pname = cmd->parameters[i]->label ? cmd->parameters[i]->label : cmd->parameters[i]->name;
    if (strcasecmp(name, pname) == 0) {
      ans = CLI_STS_PRESENT;
    }
  }
  for (i = 0; (ans == CLI_STS_ABSENT) && (i < cmd->qualifier_count); i++) {
    if (strcasecmp(name, cmd->qualifiers[i]->name) == 0) {
      ans = cmd->qualifiers[i]->negated ? CLI_STS_NEGATED : CLI_STS_PRESENT;
    }
  }
  return ans;
}

int cli_get_value(void *ctx, char *name, char **value)
{
  dclCommandPtr cmd = (dclCommandPtr) ctx;
  int i;
  int ans = CLI_STS_ABSENT;

  if (strcasecmp(name, "command_line") == 0) {
    *value = strdup(cmd->command_line);
    return 1;
  }

  for (i = 0; (ans == CLI_STS_ABSENT) && (i < cmd->parameter_count); i++) {
    if ((strcasecmp(name, cmd->parameters[i]->name) == 0) ||
	((cmd->parameters[i]->label) && (strcasecmp(name, cmd->parameters[i]->label) == 0))) {
      if (cmd->parameters[i]->value_idx >= cmd->parameters[i]->value_count) {
	ans = CLI_STS_ABSENT;
	cmd->parameters[i]->value_idx = 0;
      } else {
	*value = strdup(cmd->parameters[i]->values[cmd->parameters[i]->value_idx++]);
	ans = CLI_STS_PRESENT;
      }
    }
  }
  for (i = 0; (ans == CLI_STS_ABSENT) && (i < cmd->qualifier_count); i++) {
    if (strcasecmp(name, cmd->qualifiers[i]->name) == 0) {
      if (cmd->qualifiers[i]->value_idx >= cmd->qualifiers[i]->value_count) {
	ans = CLI_STS_ABSENT;
	cmd->qualifiers[i]->value_idx = 0;
      } else {
	*value = strdup(cmd->qualifiers[i]->values[cmd->qualifiers[i]->value_idx++]);
	ans = CLI_STS_PRESENT;
      }
    }
  }
  return ans;
}

int mdsdcl_get_input_nosymbols(char *prompt, char **input)
{
  return 1;
}

static void (*MDSDCL_OUTPUT_RTN)(char *output) = 0;

void mdsdclSetOutputRtn(void (*rtn)()) {
  MDSDCL_OUTPUT_RTN=rtn;
}

void mdsdclFlushOutput(char *output) {
  if (MDSDCL_OUTPUT_RTN) {
    MDSDCL_OUTPUT_RTN(output);
  }
}
