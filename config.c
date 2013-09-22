#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include "config.h"

typedef struct _var {
	char var[MAXLEN];			    /* variable name		*/
	union {				    /* value			*/
		char value[MAXLEN];
		int  val;
	};
} variables;

typedef struct _config {
	int id;				    /* id header		*/
	char *var;			    /* name of header		*/
	variables vr[MAXVARIABLES];	    /* variables in my header	*/
} Config;

Config ConfigIndex[] = {
	{ 0, "[general]", 
		{ { "device", 1 } }, 
	}, 
	{ 1, "[mixer]", 
		{ { "phonehead", 0 }, { "phoneviasysctl", 0 }, { "out", 0 }, }, 
	},
	{ 2, "[window]",
		{ { "pixel", 0 }, { "red", 0 }, { "green", 0 }, { "blue", 0 }, { "font", 0 } }
	}
};

int
parseconfig (char *config) {
	char str[MAXLEN];
	char variable[MAXLEN];
	char value[MAXLEN];
	int res, id=-1, error;
	int i;

	FILE* f;

	f = fopen(config, "r");
	while (!feof(f))
	{
		if(fgets(str, READFBUF, f))
		{
			res = sscanf(str, "%[^ ^\t^\n^=] = %[^\t^\n;#]", variable, value);
			if (!res) continue;
			if (res==1)
			{
				error=YES;
				for (i=0; i<INDEXLEN(ConfigIndex,Config); i++)
				{
					if (!strcmp(ConfigIndex[i].var, variable))
					{
						id = i;
						error=NO;
						break;
					}
				}
				PERROR("Config \"%s\" is not correct!\n", config);
			}
			if (res==2)
			{
				error=YES;
				for (i=0; i<MAXVARIABLES; i++)
				{
				    if (!strcmp(ConfigIndex[id].vr[i].var, variable))
				    {
					error=NO;
					strcpy(ConfigIndex[id].vr[i].value, value);
					continue;
				    };
				}
				PERROR("Config \"%s\" is not correct!\n", config);
			}
		}
		CLEAR(variable, value);
	}
	fclose(f);
	return 0;
}

const char
*get_variable(const char *global, const char *variables)
{
	int i,id;
	i=id=0;
	for (i=0; i<INDEXLEN(ConfigIndex,Config); i++)
	{
		if (!strcmp(ConfigIndex[i].var, global))
		{
		    for (id=0; id<MAXVARIABLES; id++)
		    {
			if (!strcmp(ConfigIndex[i].vr[id].var, variables))	
			{
			    return ConfigIndex[i].vr[id].value;
			}
		    }
		    break;
		}
	}
	return NULL;
}
