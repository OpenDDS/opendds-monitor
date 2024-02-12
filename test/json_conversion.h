#pragma once

#include <dds/DCPS/RapidJsonWrapper.h>
#ifndef OPENDDS_RAPIDJSON
#  define OPENDDS_RAPIDJSON
#endif
#include <dds/DCPS/JsonValueWriter.h>
#include <dds/DCPS/JsonValueReader.h>

#include <iostream>

template<typename IDL_Type, typename Writer_Type = rapidjson::Writer<rapidjson::OStreamWrapper> >
bool idl_2_json(const IDL_Type& idl_value, std::ostream& os,
    int max_decimal_places = Writer_Type::kDefaultMaxDecimalPlaces)
{
  rapidjson::OStreamWrapper osw(os);
  Writer_Type writer(osw);
  OpenDDS::DCPS::JsonValueWriter<Writer_Type> jvw(writer);
  if (max_decimal_places != Writer_Type::kDefaultMaxDecimalPlaces) {
    writer.SetMaxDecimalPlaces(max_decimal_places);
  }
  vwrite(jvw, idl_value);
  osw.Flush();
  os << std::endl;
  return true;
}
