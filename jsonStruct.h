//============JSON

#include <SmcfJsonDecoder.h>
#include "RgbLed.h"

#define DEBUG_MODE

#define JSON_ERR_INVALID_ELEMENT_TYPE JSON_ERR_USER_BASE+1 // 101
#define JSON_ERR_COLOR_EXPECTED JSON_ERR_USER_BASE+2       // 102
#define JSON_ERR_INVALID_ATTRIBUTE JSON_ERR_USER_BASE+3    // 103
#define JSON_ERR_NOT_IMPLEMENTED JSON_ERR_USER_BASE+99     // 199

// forward decls:
class JsonStep; 

typedef struct  {
  // json flow control:
  int nSteps;
  JsonStep **jStep; // até três alternativas para o próximo elemento.
  // domain data:
  RgbSetup *rgbSetup;
} JsonContext;

class JsonStep {
public:
  int expectedElementType;
  int nSteps;
  JsonStep **nextStep;

  JsonStep(int pExpectedElementType):expectedElementType(pExpectedElementType) { };
//  virtual int faz(JsonContext &jsonContext, void * value)=0;
  virtual int faz(JsonContext &jsonContext, void * value) {
    return JSON_ERR_NO_ERROR;
  }
};

class JsonColorObjEnd: public JsonStep {
  public :
  JsonColorObjEnd():JsonStep(JSON_ELEMENT_OBJECT_END) {  };

  int faz(JsonContext &jsonContext, void * value) {
     return JSON_ERR_NOT_IMPLEMENTED;    
  };
};


class JsonColorObjKey: public JsonStep {
private:
  PGM_P colorKey;
  RgbSetup *rgbSetup;
public:
  JsonColorObjKey(PGM_P pColorKey, RgbSetup *pRgbSetup):JsonStep(JSON_ELEMENT_OBJECT_KEY),colorKey(pColorKey),rgbSetup(pRgbSetup) { };

  int faz(JsonContext &jsonContext, void * value) {
      char *key = (char *) value;
      if (strcmp_P(key,colorKey)==0) {
        jsonContext.rgbSetup = rgbSetup;
        return JSON_ERR_NO_ERROR;
      }
      return JSON_ERR_COLOR_EXPECTED;
  };
};

class JsonRgbSetupObjKey:public JsonStep {
  public:
    JsonRgbSetupObjKey(): JsonStep(JSON_ELEMENT_OBJECT_KEY) {};
    
    int faz(JsonContext &jsonContext, void * value) {
          return JSON_ERR_NOT_IMPLEMENTED;
    };
};


class JsonArray {
public:
};

class jsonObjKey: public JsonStep {
private:  
  PGM_P expectedKey;
public:
  jsonObjKey(PGM_P pKey):JsonStep(JSON_ELEMENT_OBJECT_KEY), expectedKey(pKey) { };

  int faz(JsonContext &jsonContext, void * value) {
      char *key = (char *) value;
      if (strcmp_P(key,expectedKey)==0) {
        return JSON_ERR_NO_ERROR;
      }
      return JSON_ERR_INVALID_ATTRIBUTE;
  }
};

template <class T> class JsonRgbSetupValue: public JsonStep {
private:  
  T RgbSetup::*memberPtr;
public:
  JsonRgbSetupValue(int pExpectedElementType, T RgbSetup::*pMemberPtr):JsonStep(pExpectedElementType),memberPtr(pMemberPtr) { };

  int faz(JsonContext &jsonContext, void * value) {
#ifdef DEBUG_MODE
    Serial.print("valor: ");Serial.println(*((T*) value));
#endif
    jsonContext.rgbSetup->*memberPtr = *((T*) value);
    return JSON_ERR_NO_ERROR;
  }
};

/**
 * callback que decodifica o json para mudar a configuracao do rgb.
 * 
 * Formato válido: 
 * alteração completa: {  "r":{"bright":{"max":n,"min":n},"fadeAmount":n,"delay":n},"g":{"bright":{"max":n,"min":n},"fadeAmount":n,"delay":n},"b":{"bright":{"max":n,"min":n},"fadeAmount":n,"delay":n}}
 *                     
 * Se retornar qualquer coisa diferente de 0, para o processamento e retorna na hora.
 *
 */
int jsonDecoderCallbackTest(int jsonElementType, void* value, void*context) {
#ifdef DEBUG_MODE
  Serial.print(F("C:"));
  Serial.print(jsonElementType);
#endif
  JsonContext &jsonContext = *((JsonContext*)context);
  int err=JSON_ERR_INVALID_ELEMENT_TYPE;
  JsonStep *jsonStep;
  for (int s=0; s<jsonContext.nSteps; s++) {
    jsonStep = jsonContext.jStep[s];
    if (jsonElementType==jsonStep->expectedElementType) {
       err = jsonStep->faz(jsonContext, value);
       if (err==JSON_ERR_NO_ERROR) {
#ifdef DEBUG_MODE
         Serial.print(F(" - OK!"));
#endif
         jsonContext.jStep = jsonStep->nextStep;
         jsonContext.nSteps = jsonStep->nSteps;
         break; // sai do loop, pois, elemento está resolvido.
       }
    }
  }
#ifdef DEBUG_MODE
  Serial.println();
#endif
  return err; // retorna com o último erro encontrado
}

int chgLedSetupFromJson(char * jsonStr) {
/*  
  Modelo:
    char jsonStr[] = "{\"r\":{\"min\":10,\"max\":250,\"fadeAmount\":1,\"delay\":100},"+
                     "\"g\":{\"min\":11,\"max\":254,\"fadeAmount\":2,\"delay\":200},"+
                     "\"b\":{\"min\":12,\"max\":256,\"fadeAmount\":3,\"delay\":300}}";
*/
  // json structure:
  // Color JSON Object
  JsonStep jsonColorObjStart(JSON_ELEMENT_OBJECT_START);
  JsonColorObjKey jsonRedObjKey(PSTR("r"),&led[RED].setup);
  JsonColorObjKey jsonBlueObjKey(PSTR("b"),&led[BLUE].setup);
  JsonColorObjKey jsonGreenObjKey(PSTR("g"),&led[GREEN].setup);
  JsonStep jsonColorObjEnd(JSON_ELEMENT_OBJECT_END);
  JsonStep jsonRgbSetupObjStart(JSON_ELEMENT_OBJECT_START);
  JsonStep jsonRgbSetupObjEnd(JSON_ELEMENT_OBJECT_END);
  JsonStep jsonRangeObjStart(JSON_ELEMENT_OBJECT_START);
  jsonObjKey jsonMinObjKey(PSTR("min"));
  jsonObjKey jsonMaxObjKey(PSTR("max"));
  jsonObjKey jsonFadeObjKey(PSTR("fade"));
  jsonObjKey jsonDelayObjKey(PSTR("delay"));
  JsonRgbSetupValue<unsigned int> jsonMinValue(JSON_ELEMENT_NUMBER_LONG, &RgbSetup::min_brightness);
  JsonRgbSetupValue<unsigned int> jsonMaxValue(JSON_ELEMENT_NUMBER_LONG, &RgbSetup::max_brightness);
  JsonRgbSetupValue<int> jsonFadeValue(JSON_ELEMENT_NUMBER_LONG, &RgbSetup::fadeAmount);
  JsonRgbSetupValue<unsigned int> jsonDelayValue(JSON_ELEMENT_NUMBER_LONG, &RgbSetup::max_delayCount);
  JsonStep jsonRangeObjEnd(JSON_ELEMENT_OBJECT_END);
  JsonRgbSetupObjKey jsonRgbSetupObjKey;

  JsonStep *jsonColorObjNextStep[] = {&jsonRedObjKey,&jsonGreenObjKey,&jsonBlueObjKey, &jsonColorObjEnd};
  jsonColorObjStart.nSteps = 4;
  jsonColorObjStart.nextStep = jsonColorObjNextStep;
  
  // red
  JsonStep *jsonColorObjKeyNextStep[] = {&jsonRgbSetupObjStart};
  jsonRedObjKey.nSteps = 1;
  jsonRedObjKey.nextStep = jsonColorObjKeyNextStep;
  // green
  jsonGreenObjKey.nSteps = 1;
  jsonGreenObjKey.nextStep = jsonColorObjKeyNextStep;
  // blue
  jsonBlueObjKey.nSteps = 1;
  jsonBlueObjKey.nextStep = jsonColorObjKeyNextStep;

  jsonColorObjEnd.nSteps = 0;
//  jsonColorObjEnd.nextStep = {};

  // RgbSetup JSON Object
  JsonStep *jsonRgbSetupObjStartNextStep[] = {&jsonMinObjKey, &jsonMaxObjKey, &jsonFadeObjKey, &jsonDelayObjKey, &jsonRgbSetupObjEnd};
  jsonRgbSetupObjStart.nSteps = 5;
  jsonRgbSetupObjStart.nextStep = jsonRgbSetupObjStartNextStep;

  jsonRgbSetupObjEnd.nSteps = 4;
  jsonRgbSetupObjEnd.nextStep = jsonColorObjNextStep;

  // min
  JsonStep *jsonMinObjKeyNextStep[] ={&jsonMinValue};
  jsonMinObjKey.nSteps=1;
  jsonMinObjKey.nextStep = jsonMinObjKeyNextStep;
  jsonMinValue.nSteps=5;
  jsonMinValue.nextStep = jsonRgbSetupObjStartNextStep;
  // max
  JsonStep *jsonMaxObjKeyNextStep[] ={&jsonMaxValue};
  jsonMaxObjKey.nSteps=1;
  jsonMaxObjKey.nextStep = jsonMaxObjKeyNextStep;
  jsonMaxValue.nSteps=5;
  jsonMaxValue.nextStep = jsonRgbSetupObjStartNextStep;
  // fade
  JsonStep *jsonFadeObjKeyNextStep[] = {&jsonFadeValue};
  jsonFadeObjKey.nSteps=1;
  jsonFadeObjKey.nextStep = jsonFadeObjKeyNextStep;
  jsonFadeValue.nSteps=5;
  jsonFadeValue.nextStep=jsonRgbSetupObjStartNextStep;
  // delay
  JsonStep *jsonDelayObjKeyNextStep[] = {&jsonDelayValue};
  jsonDelayObjKey.nSteps=1;
  jsonDelayObjKey.nextStep = jsonDelayObjKeyNextStep;
  jsonDelayValue.nSteps=5;
  jsonDelayValue.nextStep = jsonRgbSetupObjStartNextStep;  

  SmcfJsonDecoder jsonDecoder;
  JsonStep *jsonFirstStep[] = {&jsonColorObjStart};
  JsonContext jsonContext;

  jsonContext.nSteps = 1;
  jsonContext.jStep = jsonFirstStep;  
  int err=jsonDecoder.decode(jsonStr, jsonDecoderCallbackTest, &jsonContext);

  return err;
}

void testJson() {
#ifdef DEBUG_MODE
  Serial.println(F("inicio"));
#endif
  
  char jsonStr[] = "{\"r\":{\"delay\":147,\"min\":30,\"max\":165,\"fade\":1},\"b\":{\"max\":200,\"fade\":1,\"delay\":145},\"g\":{\"max\":167,\"delay\":150,\"fade\":1}}";
  int err = chgLedSetupFromJson(jsonStr);
  
#ifdef DEBUG_MODE
  Serial.print(F("Result:"));
  Serial.println(err);

  for (int i=0; i<3; i++) {
    Serial.print(F("LED  : ")); Serial.println(i);
    RgbSetup setup= led[i].setup;
    Serial.print(F("min  : ")); Serial.println(setup.min_brightness);
    Serial.print(F("max  : ")); Serial.println(setup.max_brightness);
    Serial.print(F("fade : ")); Serial.println(setup.fadeAmount);
    Serial.print(F("delay: ")); Serial.println(setup.max_delayCount);
  }  
  
  Serial.println(F("fim"));
#endif
}

