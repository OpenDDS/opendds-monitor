#pragma once

#include "testC.h"

#include <random>
#include <sstream>

template <typename T>
std::string to_str(const T& t)
{
  std::ostringstream oss;
  oss << t;
  return oss.str();
}

void generate_bt(std::mt19937& mt, test::BasicTypes& bt)
{
  const std::string str = std::string("A basic string ") + to_str(mt());

  bt.u8 = static_cast<ACE_UINT8>(mt() % std::numeric_limits<ACE_UINT8>::max());
  bt.i8 = static_cast<ACE_INT8>(mt() % std::numeric_limits<ACE_INT8>::max());
  bt.us = static_cast<CORBA::UShort>(mt() % std::numeric_limits<CORBA::UShort>::max());
  bt.s = static_cast<CORBA::Short>(mt() % std::numeric_limits<CORBA::Short>::max());
  bt.ul = static_cast<CORBA::ULong>(mt() % std::numeric_limits<CORBA::ULong>::max());
  bt.l = static_cast<CORBA::Long>(mt() % std::numeric_limits<CORBA::Long>::max());
  bt.ull = static_cast<CORBA::ULongLong>(mt() % std::numeric_limits<CORBA::ULongLong>::max());
  bt.ll = static_cast<CORBA::LongLong>(mt() % std::numeric_limits<CORBA::LongLong>::max());
  bt.f = static_cast<CORBA::Float>(mt());
  bt.d = static_cast<CORBA::Double>(mt());
  bt.b = mt() % 2;
  bt.o = static_cast<unsigned char>(mt() % std::numeric_limits<unsigned char>::max());
  bt.c = 'a' + (mt() % 26);
  bt.wc = 'Z' - (mt() % 26);
  bt.str = str.c_str();
}

void generate_bts(std::mt19937& mt, test::BasicTypesSeq& bts)
{
  bts.length((mt() % 3));
  for (CORBA::ULong i = 0; i < bts.length(); ++i) {
    generate_bt(mt, bts[i]);
  }
}

void generate_btss(std::mt19937& mt, test::BasicTypesSeqSeq& btss)
{
  btss.length((mt() % 3));
  for (CORBA::ULong i = 0; i < btss.length(); ++i) {
    generate_bts(mt, btss[i]);
  }
}

void generate_ut(std::mt19937& mt, test::UnionType& ut)
{
  std::mt19937::result_type res = 1 + mt() % 4;
  switch (res) {
    case 1: {
      const std::string str = std::string("A union string: ") + to_str(mt());
      ut.str(str.c_str());
      break;
    }
    case 2: {
      test::BasicTypes bt;
      ut.bt(bt);
      generate_bt(mt, ut.bt());
      break;
    }
    case 3: {
      test::BasicTypesSeq bts;
      ut.bts(bts);
      generate_bts(mt, ut.bts());
      break;
    }
    case 4: {
      test::BasicTypesSeqSeq btss;
      ut.btss(btss);
      generate_btss(mt, ut.btss());
      break;
    }
  }
}

void generate_tns(std::mt19937& mt, test::TreeNodeSeq& tns);

void generate_tn(std::mt19937& mt, test::TreeNode& tn)
{
  switch (1 + mt() % 4) {
    case 1: tn.et = test::one; break;
    case 2: tn.et = test::two; break;
    case 3: tn.et = test::three; break;
    case 4: tn.et = test::four; break;
  }
  generate_ut(mt, tn.ut);
  generate_tns(mt, tn.tns);
}

void generate_tns(std::mt19937& mt, test::TreeNodeSeq& tns)
{
  tns.length((mt() % 3));
  for (CORBA::ULong i = 0; i < tns.length(); ++i) {
    generate_tn(mt, tns[i]);
  }
}

void generate_cut(std::mt19937& mt, test::ComplexUnionType& cut)
{
  std::mt19937::result_type res = 1 + mt() % 4;
  switch (res) {
    case 1: {
      const std::string str = std::string("A complex union string: ") + to_str(mt());
      cut.str(str.c_str());
      break;
    }
    case 2: {
      test::UnionType ut;
      cut.ut(ut);
      generate_ut(mt, cut.ut());
      break;
    }
    case 3: {
      test::TreeNode tn;
      cut.tn(tn);
      generate_tn(mt, cut.tn());
      break;
    }
    case 4: {
      test::TreeNodeSeq tns;
      cut.tns(tns);
      generate_tns(mt, cut.tns());
      break;
    }
  }
}

void generate_cuts(std::mt19937& mt, test::ComplexUnionTypeSeq& cuts)
{
  cuts.length((mt() % 3));
  for (CORBA::ULong i = 0; i < cuts.length(); ++i) {
    generate_cut(mt, cuts[i]);
  }
}

void generate_samples(std::mt19937& mt, CORBA::ULongLong count, test::BasicMessage& bm, test::ComplexMessage& cm)
{
  const std::string count_str = std::string("The current count is ") + to_str(count);

  bm.count = count;
  bm.bt.u8 = static_cast<ACE_UINT8>(count % std::numeric_limits<ACE_UINT8>::max());
  bm.bt.i8 = static_cast<ACE_INT8>(count % std::numeric_limits<ACE_INT8>::max());
  bm.bt.us = static_cast<CORBA::UShort>(count % std::numeric_limits<CORBA::UShort>::max());
  bm.bt.s = static_cast<CORBA::Short>(count % std::numeric_limits<CORBA::Short>::max());
  bm.bt.ul = static_cast<CORBA::ULong>(count % std::numeric_limits<CORBA::ULong>::max());
  bm.bt.l = static_cast<CORBA::Long>(count % std::numeric_limits<CORBA::Long>::max());
  bm.bt.ull = count;
  bm.bt.ll = static_cast<CORBA::LongLong>(count % std::numeric_limits<CORBA::LongLong>::max());
  bm.bt.f = static_cast<CORBA::Float>(count);
  bm.bt.d = static_cast<CORBA::Double>(count);
  bm.bt.b = count % 2;
  bm.bt.o = static_cast<unsigned char>(count % std::numeric_limits<unsigned char>::max());
  bm.bt.c = 'a' + (count % 26);
  bm.bt.wc = 'Z' - (count % 26);
  bm.bt.str = count_str.c_str();

  cm.count = count;
  cm.ct.bt.u8 = static_cast<ACE_UINT8>(count % std::numeric_limits<ACE_UINT8>::max());
  cm.ct.bt.i8 = static_cast<ACE_INT8>(count % std::numeric_limits<ACE_INT8>::max());
  cm.ct.bt.us = static_cast<CORBA::UShort>(count % std::numeric_limits<CORBA::UShort>::max());
  cm.ct.bt.s = static_cast<CORBA::Short>(count % std::numeric_limits<CORBA::Short>::max());
  cm.ct.bt.ul = static_cast<CORBA::ULong>(count % std::numeric_limits<CORBA::ULong>::max());
  cm.ct.bt.l = static_cast<CORBA::Long>(count % std::numeric_limits<CORBA::Long>::max());
  cm.ct.bt.ull = count;
  cm.ct.bt.ll = static_cast<CORBA::LongLong>(count % std::numeric_limits<CORBA::LongLong>::max());
  cm.ct.bt.f = static_cast<CORBA::Float>(count);
  cm.ct.bt.d = static_cast<CORBA::Double>(count);
  cm.ct.bt.b = count % 2;
  cm.ct.bt.o = static_cast<unsigned char>(count % std::numeric_limits<unsigned char>::max());
  cm.ct.bt.c = 'a' + (count % 26);
  cm.ct.bt.wc = 'Z' - (count % 26);
  bm.bt.str = count_str.c_str();

  generate_cuts(mt, cm.ct.cuts);
}

