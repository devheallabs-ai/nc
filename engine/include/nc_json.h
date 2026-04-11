/*
 * nc_json.h — JSON parser and serializer for NC values.
 */

#ifndef NC_JSON_H
#define NC_JSON_H

#include "nc_value.h"

NcValue  nc_json_parse(const char *json_str);
char    *nc_json_serialize(NcValue v, bool pretty);

#endif /* NC_JSON_H */
