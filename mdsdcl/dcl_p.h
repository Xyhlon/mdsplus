#ifndef DCL_P_H
#define DCL_P_H

typedef struct dclValue {
  char *value;
  char *restOfLine;
} dclValue, *dclValuePtr;

typedef struct dclValueList {
  int count;			/*!< number of values in values array */
  char *restOfLine;		/*!< rest of command line */
  char **values;		/*!< array of values */
} dclValueList, *dclValueListPtr;

typedef struct dclParameter {
  char *name;			/*!< Name of parameter for cli queries */
  char *prompt;			/*!< Prompt to present if interactive and parameter is missing but required */
  char *label;			/*!< Alternate name for cli queries */
  char *type;			/*!< Used for syntax switching */
  int required;			/*!< 1 if parameter is required */
  int value_count;		/*!< number of value string present */
  int value_idx;		/*!< index of next value to return */
  char **values;		/*!< array of values strings */
  char *restOfLine;		/*!< rest of command line */
  int useRestOfLine;		/*!< parameter uses rest of line */
  int listOk;			/*!< 1 if more than one value is allowed */
  char *help;			/*!< help string for the parameter */
} dclParameter, *dclParameterPtr;

typedef struct dclQualifier {
  char *name;			/*!< Name of qualifier for cli queries */
  int value_count;		/*!< Number of value strings present */
  int value_idx;		/*|< index of next value to return */
  char **values;		/*!< array of value string */
  int position;			/*!< position of qualifier 0=after verb,1=after 1st param, 2=after 2nd param... */
  int valueRequired;		/*!< 1 if value is required for this parameter */
  int isDefault;		/*!< 1 if the qualifier should be included by default */
  int nonnegatable;		/*!< 1 if the qualifier is not negatable. i.e. invalid if preceded by no */
  int negated;			/*!< 1 if the qualifier was include preceded by "no" */
  int listOk;			/*!< 1 if more than one value is allowed */
  char *defaultValue;		/*!< default value if none provided */
  char *type;			/*!< refers to a type of value (i.e. number, usage_type, logging_type) */
  char *syntax;			/*!< triggers syntax switching */
  char *help;			/*!< help string for qualifier */
} dclQualifier, *dclQualifierPtr;

typedef struct dclCommand {
  char *command_line;		/*!< full command line */
  int rest_of_line;		/*!< command handler deals with parsing the command.
				   Parameters and qualifiers not checked generically. */
  char *verb;			/*!< verb name of command */
  int parameter_count;		/*!< number of parameters */
  dclParameterPtr *parameters;	/*!< array of parameters */
  int qualifier_count;		/*!< number of qualifiers */
  dclQualifierPtr *qualifiers;	/*!< array of qualifiers */
  char *routine;		/*!< routine providing implementation of the command */
} dclCommand, *dclCommandPtr;

#define YYSTYPE_IS_DECLARED
typedef union YYSTYPE {		/*!< Used for types used during bison/flex parsing of commands */
  char *str;
  dclCommandPtr cmd;
  dclQualifierPtr qualifier;
  dclValueListPtr value_list;
  dclParameterPtr parameter;
  dclValuePtr pvalue;
} YYSTYPE;

typedef struct dclNodeList {
  int count;			/*!< number of nodes in nodes array */
  void **nodes;			/*!< array of xmlNodePtr's */
} dclNodeList, *dclNodeListPtr;

typedef struct dclDocList {
  char *name;			/*!< Name of xml command definition (i.e. tcl_commands) */
  void *doc;			/*!< xml document specifying the command syntax */
  struct dclDocList *next;	/*!< Next loaded document */
} dclDocList, *dclDocListPtr;

extern dclDocListPtr mdsdcl_getdocs();
extern char *mdsdclGetPrompt();
extern char *mdsdclGetHistoryFile();
extern int mdsdcl_do_command_extra_args(char const* command, char **prompt, char **output, char **error, char *(*getline)(), void *getlineinfo);
extern int mdsdclAddCommands(char *name_in, char **error);

#endif
