#pragma once

/* Numeric value indicating the current major version of the API.
 * This value must be incremented each time a breaking change to
 * the API is introduced(change of existing API methods/properties
 * signatures).
 */
#define HOST_API_VERSION_MAJOR 1

/* Numeric value indicating the current minor version of the API.
 * This value will be incremented each time a non-breaking change
 * to the API is introduced (additional functionality, bugfixes
 * of existing functionality).
 */
#define HOST_API_VERSION_MINOR 0

/* Numeric value in the YYYYMMDDHHmmss format, indicating the current
 * version of the plugin.
 *
 * This version number is used by obs-streamelements plug-in to
 * determine whether an update is available and should be offered to
 * the user.
 */
//#define STREAMELEMENTS_PLUGIN_VERSION 20180506015800L
#define STREAMELEMENTS_PLUGIN_VERSION 20192536415800L
