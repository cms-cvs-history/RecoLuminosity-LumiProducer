#ifndef RecoLuminosity_LumiProducer_TRG2DB_h 
#define RecoLuminosity_LumiProducer_TRG2DB_h 
#include "CoralBase/AttributeList.h"
#include "CoralBase/Attribute.h"
#include "CoralBase/AttributeSpecification.h"
#include "CoralBase/Blob.h"
#include "CoralBase/Exception.h"
#include "RelationalAccess/ConnectionService.h"
#include "RelationalAccess/ISessionProxy.h"
#include "RelationalAccess/ITransaction.h"
#include "RelationalAccess/ITypeConverter.h"
#include "RelationalAccess/IQuery.h"
#include "RelationalAccess/ICursor.h"
#include "RelationalAccess/ISchema.h"
#include "RelationalAccess/IView.h"
#include "RelationalAccess/ITable.h"
#include "RelationalAccess/ITableDataEditor.h"
#include "RelationalAccess/IBulkOperation.h"
#include "RecoLuminosity/LumiProducer/interface/DataPipe.h"
#include "RecoLuminosity/LumiProducer/interface/LumiNames.h"
#include "RecoLuminosity/LumiProducer/interface/idDealer.h"
#include "RecoLuminosity/LumiProducer/interface/Exception.h"
#include "RecoLuminosity/LumiProducer/interface/DBConfig.h"
#include "RecoLuminosity/LumiProducer/interface/ConstantDef.h"
#include "RecoLuminosity/LumiProducer/interface/RevisionDML.h"
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <map>
namespace lumi{
  class TRG2DB : public DataPipe{
  public:
    const static unsigned int COMMITLSINTERVAL=150; //commit interval in LS, totalrow=nsl*192
    const static unsigned int COMMITLSTRGINTERVAL=550; //commit interval in LS of schema2
    explicit TRG2DB(const std::string& dest);
    virtual void retrieveData( unsigned int runnumber);
    virtual const std::string dataType() const;
    virtual const std::string sourceType() const;
    virtual ~TRG2DB();													  

    //per run information
    typedef std::vector<std::string> TriggerNameResult_Algo;
    typedef std::vector<std::string> TriggerNameResult_Tech;
    //per lumisection information
    typedef unsigned long long DEADCOUNT;
    typedef std::vector<DEADCOUNT> TriggerDeadCountResult;
    typedef std::vector<unsigned int> BITCOUNT;
    typedef std::vector<BITCOUNT> TriggerCountResult_Algo;
    typedef std::vector<BITCOUNT> TriggerCountResult_Tech;
    typedef std::map< unsigned int, std::vector<unsigned int> > PrescaleResult_Algo;
    typedef std::map< unsigned int, std::vector<unsigned int> > PrescaleResult_Tech;

    std::string int2str(unsigned int t,unsigned int width);
    unsigned int str2int(const std::string& s);
    void writeTrgData(coral::ISessionProxy* session,
		      unsigned int runnumber,
		      const std::string& source,
		      TriggerDeadCountResult::iterator deadtimesBeg,
		      TriggerDeadCountResult::iterator deadtimesEnd,
		      TriggerNameResult_Algo& algonames, 
		      TriggerNameResult_Tech& technames,
		      TriggerCountResult_Algo& algocounts,
		      TriggerCountResult_Tech& techcounts,
		      PrescaleResult_Algo& prescalealgo,
		      PrescaleResult_Tech& prescaletech,
		      unsigned int commitintv);
    void writeTrgDataToSchema2(coral::ISessionProxy* session,
			       unsigned int runnumber,
			       const std::string& source,
			       TriggerDeadCountResult::iterator deadtimesBeg,
			       TriggerDeadCountResult::iterator deadtimesEnd,
			       TriggerNameResult_Algo& algonames,
			       TriggerNameResult_Tech& technames,
			       TriggerCountResult_Algo& algocounts,
			       TriggerCountResult_Tech& techcounts,
			       PrescaleResult_Algo& prescalealgo, 
			       PrescaleResult_Tech& prescaletech,
			       unsigned int commitintv);
  };//cl TRG2DB
  //
  //implementation
  //
 TRG2DB::TRG2DB(const std::string& dest):DataPipe(dest){}
  void TRG2DB::retrieveData( unsigned int runnumber){
    std::string runnumberstr=int2str(runnumber,6);
    //query source GT database
    coral::ConnectionService* svc=new coral::ConnectionService;
    lumi::DBConfig dbconf(*svc);
    if(!m_authpath.empty()){
      dbconf.setAuthentication(m_authpath);
    }
    //std::cout<<"m_source "<<m_source<<std::endl;
    coral::ISessionProxy* trgsession=svc->connect(m_source, coral::ReadOnly);
    coral::ITypeConverter& tpc=trgsession->typeConverter();

    tpc.setCppTypeForSqlType("unsigned int","NUMBER(7)");
    tpc.setCppTypeForSqlType("unsigned int","NUMBER(10)");
    tpc.setCppTypeForSqlType("unsigned long long","NUMBER(20)");
    
    coral::AttributeList bindVariableList;
    bindVariableList.extend("runnumber",typeid(unsigned int));
    bindVariableList["runnumber"].data<unsigned int>()=runnumber;
    std::string gtmonschema("CMS_GT_MON");
    std::string algoviewname("GT_MON_TRIG_ALGO_VIEW");
    std::string techviewname("GT_MON_TRIG_TECH_VIEW");
    std::string deadviewname("GT_MON_TRIG_DEAD_VIEW");
    std::string celltablename("GT_CELL_LUMISEG");
    std::string lstablename("LUMI_SECTIONS");

    std::string gtschema("CMS_GT");
    std::string runtechviewname("GT_RUN_TECH_VIEW");
    std::string runalgoviewname("GT_RUN_ALGO_VIEW");
    std::string runprescalgoviewname("GT_RUN_PRESC_ALGO_VIEW");
    std::string runpresctechviewname("GT_RUN_PRESC_TECH_VIEW");

    //data exchange format
    lumi::TRG2DB::PrescaleResult_Algo algoprescale;
    lumi::TRG2DB::PrescaleResult_Tech techprescale;
    lumi::TRG2DB::BITCOUNT mybitcount_algo;
    mybitcount_algo.reserve(lumi::N_TRGALGOBIT);
    lumi::TRG2DB::BITCOUNT mybitcount_tech; 
    mybitcount_tech.reserve(lumi::N_TRGTECHBIT);
    lumi::TRG2DB::TriggerNameResult_Algo algonames;
    algonames.reserve(lumi::N_TRGALGOBIT);
    lumi::TRG2DB::TriggerNameResult_Tech technames;
    technames.reserve(lumi::N_TRGTECHBIT);
    lumi::TRG2DB::TriggerCountResult_Algo algocount;
    //algocount.reserve(400);
    lumi::TRG2DB::TriggerCountResult_Tech techcount;
    //techcount.reserve(400);
    lumi::TRG2DB::TriggerDeadCountResult deadtimeresult;
    deadtimeresult.reserve(400);
    coral::ITransaction& transaction=trgsession->transaction();
    transaction.start(true);
    //uncomment if you want to see all the visible views
    /**
       transaction.start(true); //true means readonly transaction
       std::cout<<"schema name "<<session->schema(gtmonschema).schemaName()<<std::endl;
       std::set<std::string> listofviews;
       listofviews=session->schema(gtmonschema).listViews();
       for( std::set<std::string>::iterator it=listofviews.begin(); it!=listofviews.end();++it ){
       std::cout<<"view: "<<*it<<std::endl;
       } 
       std::cout<<"schema name "<<session->schema(gtschema).schemaName()<<std::endl;
       listofviews.clear();
       listofviews=session->schema(gtschema).listViews();
       for( std::set<std::string>::iterator it=listofviews.begin(); it!=listofviews.end();++it ){
       std::cout<<"view: "<<*it<<std::endl;
       } 
       std::cout<<"commit transaction"<<std::endl;
       transaction.commit();
    **/
    /**
       Part I
       query tables in schema cms_gt_mon
    **/
    coral::ISchema& gtmonschemaHandle=trgsession->schema(gtmonschema);    
    if(!gtmonschemaHandle.existsView(algoviewname)){
      throw lumi::Exception(std::string("non-existing view ")+algoviewname,"retrieveData","TRG2DB");
    }
    if(!gtmonschemaHandle.existsView(techviewname)){
      throw lumi::Exception(std::string("non-existing view ")+techviewname,"retrieveData","TRG2DB");
    }
    if(!gtmonschemaHandle.existsView(deadviewname)){
      throw lumi::Exception(std::string("non-existing view ")+deadviewname,"retrieveData","TRG2DB");
    }
    if(!gtmonschemaHandle.existsTable(celltablename)){
      throw lumi::Exception(std::string("non-existing table ")+celltablename,"retrieveData","TRG2DB");
    }
    //
    //select counts,lsnr,algobit from cms_gt_mon.gt_mon_trig_algo_view where runnr=:runnumber order by lsnr,algobit;
    //note: algobit count from 0-127
    //note: lsnr count from 1 
    //
    coral::IQuery* Queryalgoview=gtmonschemaHandle.newQuery();
    Queryalgoview->addToTableList(algoviewname);
    coral::AttributeList qalgoOutput;
    qalgoOutput.extend("counts",typeid(unsigned int));
    qalgoOutput.extend("lsnr",typeid(unsigned int));
    qalgoOutput.extend("algobit",typeid(unsigned int));
    Queryalgoview->addToOutputList("counts");
    Queryalgoview->addToOutputList("lsnr");
    Queryalgoview->addToOutputList("algobit");
    Queryalgoview->setCondition("RUNNR =:runnumber",bindVariableList);
    Queryalgoview->addToOrderList("lsnr");
    Queryalgoview->addToOrderList("algobit");
    Queryalgoview->defineOutput(qalgoOutput);
    coral::ICursor& c=Queryalgoview->execute();
    
    unsigned int s=0;
    while( c.next() ){
      const coral::AttributeList& row = c.currentRow();     
      //row.toOutputStream( std::cout ) << std::endl;
      unsigned int lsnr=row["lsnr"].data<unsigned int>();
      unsigned int count=row["counts"].data<unsigned int>();
      unsigned int algobit=row["algobit"].data<unsigned int>();
      mybitcount_algo.push_back(count);
      if(algobit==(lumi::N_TRGALGOBIT-1)){
	++s;
	while(s!=lsnr){
	  std::cout<<"ALGO COUNT alert: found hole in LS range"<<std::endl;
	  std::cout<<"    fill all algocount 0 for LS "<<s<<std::endl;
	  std::vector<unsigned int> tmpzero(lumi::N_TRGALGOBIT,0);
	  algocount.push_back(tmpzero);
	  ++s;
	}
	algocount.push_back(mybitcount_algo);
	mybitcount_algo.clear();
      }
    }
    if(s==0){
      c.close();
      delete Queryalgoview;
      transaction.commit();
      throw lumi::Exception(std::string("requested run ")+runnumberstr+std::string(" doesn't exist for algocounts"),"retrieveData","TRG2DB");
    }
    delete Queryalgoview;
    //
    //select counts,lsnr,techbit from cms_gt_mon.gt_mon_trig_tech_view where runnr=:runnumber order by lsnr,techbit;
    //note: techobit count from 0-63
    //note: lsnr count from 1 
    //
    coral::IQuery* Querytechview=gtmonschemaHandle.newQuery();
    Querytechview->addToTableList(techviewname);
    coral::AttributeList qtechOutput;
    qtechOutput.extend("counts",typeid(unsigned int));
    qtechOutput.extend("lsnr",typeid(unsigned int));
    qtechOutput.extend("techbit",typeid(unsigned int));
    Querytechview->addToOutputList("counts");
    Querytechview->addToOutputList("lsnr");
    Querytechview->addToOutputList("techbit");
    Querytechview->setCondition("RUNNR =:runnumber",bindVariableList);
    Querytechview->addToOrderList("lsnr");
    Querytechview->addToOrderList("techbit");
    Querytechview->defineOutput(qtechOutput);
    coral::ICursor& techcursor=Querytechview->execute();
    
    s=0;
    while( techcursor.next() ){
      const coral::AttributeList& row = techcursor.currentRow();     
      //row.toOutputStream( std::cout ) << std::endl;
      unsigned int lsnr=row["lsnr"].data<unsigned int>();
      unsigned int count=row["counts"].data<unsigned int>();
      unsigned int techbit=row["techbit"].data<unsigned int>();
      mybitcount_tech.push_back(count);
      if(techbit==(lumi::N_TRGTECHBIT-1)){
	++s;
	while(s!=lsnr){
	  std::cout<<"TECH COUNT alert: found hole in LS range"<<std::endl;
	  std::cout<<"     fill all techcount with 0 for LS "<<s<<std::endl;
	  std::vector<unsigned int> tmpzero(lumi::N_TRGTECHBIT,0);
	  techcount.push_back(tmpzero);
	  ++s;
	}
	techcount.push_back(mybitcount_tech);
	mybitcount_tech.clear();
      }
    }
    if(s==0){
      techcursor.close();
      delete Querytechview;
      transaction.commit();
      throw lumi::Exception(std::string("requested run ")+runnumberstr+std::string(" doesn't exist for techcounts"),"retrieveData","TRG2DB");
    }
    delete Querytechview;
    //
    //select counts,lsnr from cms_gt_mon.gt_mon_trig_dead_view where runnr=:runnumber and deadcounter=:countername order by lsnr;
    //
    //note: lsnr count from 1 
    //
    coral::IQuery* Querydeadview=gtmonschemaHandle.newQuery();
    Querydeadview->addToTableList(deadviewname);
    coral::AttributeList qdeadOutput;
    qdeadOutput.extend("counts",typeid(unsigned int));
    qdeadOutput.extend("lsnr",typeid(unsigned int));
    Querydeadview->addToOutputList("counts");
    Querydeadview->addToOutputList("lsnr");
    coral::AttributeList bindVariablesDead;
    bindVariablesDead.extend("runnumber",typeid(int));
    bindVariablesDead.extend("countername",typeid(std::string));
    bindVariablesDead["runnumber"].data<int>()=runnumber;
    bindVariablesDead["countername"].data<std::string>()=std::string("DeadtimeBeamActive");
    Querydeadview->setCondition("RUNNR =:runnumber AND DEADCOUNTER =:countername",bindVariablesDead);
    Querydeadview->addToOrderList("lsnr");
    Querydeadview->defineOutput(qdeadOutput);
    coral::ICursor& deadcursor=Querydeadview->execute();
    s=0;
    while( deadcursor.next() ){
      const coral::AttributeList& row = deadcursor.currentRow();     
      //row.toOutputStream( std::cout ) << std::endl;
      ++s;
      unsigned int lsnr=row["lsnr"].data<unsigned int>();
      while(s!=lsnr){
	std::cout<<"DEADTIME alert: found hole in LS range"<<std::endl;
	std::cout<<"         fill deadtimebeamactive 0 for LS "<<s<<std::endl;
	deadtimeresult.push_back(0);
	++s;
      }
      unsigned int count=row["counts"].data<unsigned int>();
      deadtimeresult.push_back(count);
    }
    if(s==0){
      deadcursor.close();
      delete Querydeadview;
      transaction.commit();
      throw lumi::Exception(std::string("requested run ")+runnumberstr+std::string(" doesn't exist for deadcounts"),"retrieveData","TRG2DB");
      return;
    }
    delete Querydeadview;

    //
    //select distinct(prescale_index) from cms_gt_mon.lumi_sections where run_number=:runnumber;
    //
    std::vector< int > prescidx;
    coral::IQuery* allpsidxQuery=gtmonschemaHandle.newQuery();
    allpsidxQuery->addToTableList(lstablename);
    coral::AttributeList allpsidxOutput;
    allpsidxOutput.extend("psidx",typeid(int));
    allpsidxQuery->addToOutputList("distinct PRESCALE_INDEX","psidx");
    coral::AttributeList bindVariablesAllpsidx;
    bindVariablesAllpsidx.extend("runnumber",typeid(int));
    bindVariablesAllpsidx["runnumber"].data<int>()=runnumber;
    allpsidxQuery->setCondition("RUN_NUMBER =:runnumber",bindVariablesAllpsidx);
    allpsidxQuery->defineOutput(allpsidxOutput);
    coral::ICursor& allpsidxCursor=allpsidxQuery->execute();
    while( allpsidxCursor.next() ){
      const coral::AttributeList& row = allpsidxCursor.currentRow();     
      int psidx=row["psidx"].data<int>();
      prescidx.push_back(psidx);
    }
    delete allpsidxQuery;
    std::map< int, std::vector<unsigned int> > algoprescMap;
    std::map< int, std::vector<unsigned int> > techprescMap;
    std::vector< int >::iterator prescidxIt;
    std::vector< int >::iterator prescidxItBeg=prescidx.begin();
    std::vector< int >::iterator prescidxItEnd=prescidx.end();
    for(prescidxIt=prescidxItBeg;prescidxIt!=prescidxItEnd;++prescidxIt){
      std::vector<unsigned int> algopres; algopres.reserve(lumi::N_TRGALGOBIT);
      std::vector<unsigned int> techpres; techpres.reserve(lumi::N_TRGTECHBIT);
      algoprescMap.insert(std::make_pair(*prescidxIt,algopres));
      techprescMap.insert(std::make_pair(*prescidxIt,techpres));
    }
    //
    //select lumi_section,prescale_index from cms_gt_mon.lumi_sections where run_number=:runnumber
    // {ls:prescale_index}
    //
    std::map< unsigned int, int > lsprescmap;
    coral::IQuery* lstoprescQuery=gtmonschemaHandle.newQuery();
    lstoprescQuery->addToTableList(lstablename);
    coral::AttributeList lstoprescOutput;
    lstoprescOutput.extend("lumisection",typeid(unsigned int));
    lstoprescOutput.extend("psidx",typeid(int));
    lstoprescQuery->addToOutputList("LUMI_SECTION","lumisection");
    lstoprescQuery->addToOutputList("PRESCALE_INDEX","psidx");
    coral::AttributeList bindVariablesLstopresc;
    bindVariablesLstopresc.extend("runnumber",typeid(int));
    bindVariablesLstopresc["runnumber"].data<int>()=runnumber;
    lstoprescQuery->setCondition("RUN_NUMBER =:runnumber",bindVariablesLstopresc);
    lstoprescQuery->defineOutput(lstoprescOutput);
    coral::ICursor& lstoprescCursor=lstoprescQuery->execute();
    while( lstoprescCursor.next() ){
      const coral::AttributeList& row = lstoprescCursor.currentRow();     
      unsigned int lumisection=row["lumisection"].data< unsigned int>();
      int psidx=row["psidx"].data< int>();
      lsprescmap.insert(std::make_pair(lumisection,psidx));
    }
    delete lstoprescQuery ;

    for(prescidxIt=prescidxItBeg;prescidxIt!=prescidxItEnd;++prescidxIt){
      std::vector<unsigned int> algopres; algopres.reserve(lumi::N_TRGALGOBIT);
      std::vector<unsigned int> techpres; techpres.reserve(lumi::N_TRGTECHBIT);
      algoprescMap.insert(std::make_pair(*prescidxIt,algopres));
      techprescMap.insert(std::make_pair(*prescidxIt,techpres));
    }
    //prefill lsprescmap
    //transaction.commit();
    /**
       Part II
       query tables in schema cms_gt
    **/
    coral::ISchema& gtschemaHandle=trgsession->schema(gtschema);
    if(!gtschemaHandle.existsView(runtechviewname)){
      throw lumi::Exception(std::string("non-existing view ")+runtechviewname,"str2int","TRG2DB");
    }
    if(!gtschemaHandle.existsView(runalgoviewname)){
      throw lumi::Exception(std::string("non-existing view ")+runalgoviewname,"str2int","TRG2DB");
    }
    if(!gtschemaHandle.existsView(runprescalgoviewname)){
      throw lumi::Exception(std::string("non-existing view ")+runprescalgoviewname,"str2int","TRG2DB");
    }
    if(!gtschemaHandle.existsView(runpresctechviewname)){
      throw lumi::Exception(std::string("non-existing view ")+runpresctechviewname,"str2int","TRG2DB");
    }
    //
    //select algo_index,alias from cms_gt.gt_run_algo_view where runnumber=:runnumber //order by algo_index;
    //
    std::map<unsigned int,std::string> triggernamemap;
    coral::IQuery* QueryName=gtschemaHandle.newQuery();
    QueryName->addToTableList(runalgoviewname);
    coral::AttributeList qAlgoNameOutput;
    qAlgoNameOutput.extend("algo_index",typeid(unsigned int));
    qAlgoNameOutput.extend("alias",typeid(std::string));
    QueryName->addToOutputList("algo_index");
    QueryName->addToOutputList("alias");
    QueryName->setCondition("runnumber =:runnumber",bindVariableList);
    //QueryName->addToOrderList("algo_index");
    QueryName->defineOutput(qAlgoNameOutput);
    coral::ICursor& algonamecursor=QueryName->execute();
    while( algonamecursor.next() ){
      const coral::AttributeList& row = algonamecursor.currentRow();     
      //row.toOutputStream( std::cout ) << std::endl;
      unsigned int algo_index=row["algo_index"].data<unsigned int>();
      std::string algo_name=row["alias"].data<std::string>();
      triggernamemap.insert(std::make_pair(algo_index,algo_name));
    }
    delete QueryName;
    
    //
    //select techtrig_index,name from cms_gt.gt_run_tech_view where runnumber=:runnumber //order by techtrig_index;
    //
    std::map<unsigned int,std::string> techtriggernamemap;
    coral::IQuery* QueryTechName=gtschemaHandle.newQuery();
    QueryTechName->addToTableList(runtechviewname);
    coral::AttributeList qTechNameOutput;
    qTechNameOutput.extend("techtrig_index",typeid(unsigned int));
    qTechNameOutput.extend("name",typeid(std::string));
    QueryTechName->addToOutputList("techtrig_index");
    QueryTechName->addToOutputList("name");
    QueryTechName->setCondition("runnumber =:runnumber",bindVariableList);
    //QueryTechName->addToOrderList("techtrig_index");
    QueryTechName->defineOutput(qTechNameOutput);
    coral::ICursor& technamecursor=QueryTechName->execute();
    while( technamecursor.next() ){
      const coral::AttributeList& row = technamecursor.currentRow();     
      //row.toOutputStream( std::cout ) << std::endl;
      unsigned int tech_index=row["techtrig_index"].data<unsigned int>();
      std::string tech_name=row["name"].data<std::string>();
      techtriggernamemap.insert(std::make_pair(tech_index,tech_name));
    }
    delete QueryTechName;
    //
    //loop over all prescale_index
    //
    //select prescale_factor_algo_000,prescale_factor_algo_001..._127 from cms_gt.gt_run_presc_algo_view where runr=:runnumber and prescale_index=:prescale_index;
    // {prescale_index:[pres]}
    //
    std::vector< int >::iterator presIt;
    std::vector< int >::iterator presItBeg=prescidx.begin();
    std::vector< int >::iterator presItEnd=prescidx.end();
    for( presIt=presItBeg; presIt!=presItEnd; ++presIt ){
      coral::IQuery* QueryAlgoPresc=gtschemaHandle.newQuery();
      QueryAlgoPresc->addToTableList(runprescalgoviewname);
      coral::AttributeList qAlgoPrescOutput;
      std::string algoprescBase("PRESCALE_FACTOR_ALGO_");
      for(unsigned int bitidx=0;bitidx<lumi::N_TRGALGOBIT;++bitidx){
	std::string algopresc=algoprescBase+int2str(bitidx,3);
	qAlgoPrescOutput.extend(algopresc,typeid(unsigned int));
      }
      for(unsigned int bitidx=0;bitidx<lumi::N_TRGALGOBIT;++bitidx){
	std::string algopresc=algoprescBase+int2str(bitidx,3);
	QueryAlgoPresc->addToOutputList(algopresc);
      }
      coral::AttributeList PrescbindVariable;
      PrescbindVariable.extend("runnumber",typeid(int));
      PrescbindVariable.extend("prescaleindex",typeid(int));
      PrescbindVariable["runnumber"].data<int>()=runnumber;
      PrescbindVariable["prescaleindex"].data<int>()=*presIt;
      QueryAlgoPresc->setCondition("runnr =:runnumber AND prescale_index =:prescaleindex",PrescbindVariable);
      QueryAlgoPresc->defineOutput(qAlgoPrescOutput);
      coral::ICursor& algopresccursor=QueryAlgoPresc->execute();
      while( algopresccursor.next() ){
	const coral::AttributeList& row = algopresccursor.currentRow();     
	for(unsigned int bitidx=0;bitidx<lumi::N_TRGALGOBIT;++bitidx){
	  std::string algopresc=algoprescBase+int2str(bitidx,3);
	  algoprescMap[*presIt].push_back(row[algopresc].data<unsigned int>());
	}
      }
      delete QueryAlgoPresc;
    }
    //
    //select prescale_factor_tt_000,prescale_factor_tt_001..._63 from cms_gt.gt_run_presc_tech_view where runr=:runnumber and prescale_index=0;
    //    
    for( presIt=presItBeg; presIt!=presItEnd; ++presIt ){
      coral::IQuery* QueryTechPresc=gtschemaHandle.newQuery();
      QueryTechPresc->addToTableList(runpresctechviewname);
      coral::AttributeList qTechPrescOutput;
      std::string techprescBase("PRESCALE_FACTOR_TT_");
      for(unsigned int bitidx=0;bitidx<lumi::N_TRGTECHBIT;++bitidx){
	std::string techpresc=techprescBase+this->int2str(bitidx,3);
	qTechPrescOutput.extend(techpresc,typeid(unsigned int));
      }
      for(unsigned int bitidx=0;bitidx<lumi::N_TRGTECHBIT;++bitidx){
	std::string techpresc=techprescBase+int2str(bitidx,3);
	QueryTechPresc->addToOutputList(techpresc);
      }
      coral::AttributeList TechPrescbindVariable;
      TechPrescbindVariable.extend("runnumber",typeid(int));
      TechPrescbindVariable.extend("prescaleindex",typeid(int));
      TechPrescbindVariable["runnumber"].data<int>()=runnumber;
      TechPrescbindVariable["prescaleindex"].data<int>()=*presIt;
      QueryTechPresc->setCondition("runnr =:runnumber AND prescale_index =:prescaleindex",TechPrescbindVariable);
      QueryTechPresc->defineOutput(qTechPrescOutput);
      coral::ICursor& techpresccursor=QueryTechPresc->execute();
      while( techpresccursor.next() ){
	const coral::AttributeList& row = techpresccursor.currentRow();     
	//row.toOutputStream( std::cout ) << std::endl;
	for(unsigned int bitidx=0;bitidx<lumi::N_TRGTECHBIT;++bitidx){
	  std::string techpresc=techprescBase+int2str(bitidx,3);
	  techprescMap[*presIt].push_back(row[techpresc].data<unsigned int>());
	}
      }
      delete QueryTechPresc;
    }
    transaction.commit();
    delete trgsession;

    std::map< unsigned int, int >::iterator lsprescmapIt;
    std::map< unsigned int, int >::iterator lsprescmapItBeg=lsprescmap.begin();
    std::map< unsigned int, int >::iterator lsprescmapItEnd=lsprescmap.end();
    for( lsprescmapIt=lsprescmapItBeg; lsprescmapIt!=lsprescmapItEnd; ++lsprescmapIt ){
      unsigned int ls=lsprescmapIt->first;
      int preidx=lsprescmapIt->second;
      algoprescale.insert(std::make_pair(ls,algoprescMap[preidx]));
      techprescale.insert(std::make_pair(ls,techprescMap[preidx]));
    }
    algoprescMap.clear();
    techprescMap.clear();
    //
    //reprocess Algo name result filling unallocated trigger bit with string "False"
    //
    for(size_t algoidx=0;algoidx<lumi::N_TRGALGOBIT;++algoidx){
      std::map<unsigned int,std::string>::iterator pos=triggernamemap.find(algoidx);
      if(pos!=triggernamemap.end()){
	algonames.push_back(pos->second);
      }else{
	algonames.push_back("False");
      }
    }
    //
    //reprocess Tech name result filling unallocated trigger bit with string "False"  
    //
    std::stringstream ss;
    for(size_t techidx=0;techidx<lumi::N_TRGTECHBIT;++techidx){
      std::map<unsigned int,std::string>::iterator pos=techtriggernamemap.find(techidx);
      ss<<techidx;
      technames.push_back(ss.str());
      ss.str(""); //clear the string buffer after usage
    }
    //
    //cross check result size
    //
    if(algonames.size()!=lumi::N_TRGALGOBIT || technames.size()!=lumi::N_TRGTECHBIT){
      throw lumi::Exception("wrong number of bits","retrieveData","TRG2DB");
    }
    //if(algoprescale.size()!=lumi::N_TRGALGOBIT || techprescale.size()!=lumi::N_TRGTECHBIT){
    //  throw lumi::Exception("wrong number of prescale","retrieveData","TRG2DB");
    //}
    if(deadtimeresult.size()!=algocount.size() || deadtimeresult.size()!=techcount.size()){
      throw lumi::Exception("inconsistent number of LS","retrieveData","TRG2DB");
    }
    //    
    //write data into lumi db
    //
    
    coral::ISessionProxy* lumisession=svc->connect(m_dest,coral::Update);
    coral::ITypeConverter& lumitpc=lumisession->typeConverter();
    lumitpc.setCppTypeForSqlType("unsigned int","NUMBER(7)");
    lumitpc.setCppTypeForSqlType("unsigned int","NUMBER(10)");
    lumitpc.setCppTypeForSqlType("unsigned long long","NUMBER(20)");
    try{
      std::cout<<"writing trg data to old trg table "<<std::endl;
      writeTrgData(lumisession,runnumber,m_source,deadtimeresult.begin(),deadtimeresult.end(),algonames,technames,algocount,techcount,algoprescale,techprescale,COMMITLSINTERVAL);
      std::cout<<"done"<<std::endl;
      std::cout<<"writing trg data to new lstrg table "<<std::endl;
      writeTrgDataToSchema2(lumisession,runnumber,m_source,deadtimeresult.begin(),deadtimeresult.end(),algonames,technames,algocount,techcount,algoprescale,techprescale,COMMITLSTRGINTERVAL);
      std::cout<<"done"<<std::endl;
      delete lumisession;
      delete svc;
    }catch( const coral::Exception& er){
      std::cout<<"database error "<<er.what()<<std::endl;
      lumisession->transaction().rollback();
      delete lumisession;
      delete svc;
      throw er;
    }
  }
  void  
  TRG2DB::writeTrgData(coral::ISessionProxy* lumisession,
		       unsigned int runnumber,
		       const std::string& source,
		       TRG2DB::TriggerDeadCountResult::iterator deadtimesBeg,TRG2DB::TriggerDeadCountResult::iterator deadtimesEnd,
		       TRG2DB::TriggerNameResult_Algo& algonames,
		       TRG2DB::TriggerNameResult_Tech& technames,
		       TRG2DB::TriggerCountResult_Algo& algocounts,
		       TRG2DB::TriggerCountResult_Tech& techcounts,
		       TRG2DB::PrescaleResult_Algo& prescalealgo,
		       TRG2DB::PrescaleResult_Tech& prescaletech,
		       unsigned int commitintv){    
    TRG2DB::TriggerDeadCountResult::iterator deadIt;
    //unsigned int totalcmsls=deadtimes.size();
    unsigned int totalcmsls=std::distance(deadtimesBeg,deadtimesEnd);
    std::cout<<"inserting totalcmsls "<<totalcmsls<<std::endl;
    std::map< unsigned long long, std::vector<unsigned long long> > idallocationtable;

    std::cout<<"\t allocating total ids "<<totalcmsls*lumi::N_TRGBIT<<std::endl; 
    lumisession->transaction().start(false);
    lumi::idDealer idg(lumisession->nominalSchema());
    unsigned long long trgID = idg.generateNextIDForTable(LumiNames::trgTableName(),totalcmsls*lumi::N_TRGBIT)-totalcmsls*lumi::N_TRGBIT;
    //lumisession->transaction().commit();
    unsigned int trglscount=0;
    for(deadIt=deadtimesBeg;deadIt!=deadtimesEnd;++deadIt,++trglscount){
      std::vector<unsigned long long> bitvec;
      bitvec.reserve(lumi::N_TRGBIT);
      const BITCOUNT& algoinbits=algocounts[trglscount];
      const BITCOUNT& techinbits=techcounts[trglscount];
      BITCOUNT::const_iterator algoBitIt;
      BITCOUNT::const_iterator algoBitBeg=algoinbits.begin();
      BITCOUNT::const_iterator algoBitEnd=algoinbits.end();	
      for(algoBitIt=algoBitBeg;algoBitIt!=algoBitEnd;++algoBitIt,++trgID){
	bitvec.push_back(trgID);
      }
      BITCOUNT::const_iterator techBitIt;
      BITCOUNT::const_iterator techBitBeg=techinbits.begin();
      BITCOUNT::const_iterator techBitEnd=techinbits.end();
      for(techBitIt=techBitBeg;techBitIt!=techBitEnd;++techBitIt,++trgID){
	bitvec.push_back(trgID);
      }
      idallocationtable.insert(std::make_pair(trglscount,bitvec));
    }
    std::cout<<"\t all ids allocated"<<std::endl; 
    coral::AttributeList trgData;
    trgData.extend<unsigned long long>("TRG_ID");
    trgData.extend<unsigned int>("RUNNUM");
    trgData.extend<unsigned int>("CMSLSNUM");
    trgData.extend<unsigned int>("BITNUM");
    trgData.extend<std::string>("BITNAME");
    trgData.extend<unsigned int>("TRGCOUNT");
    trgData.extend<unsigned long long>("DEADTIME");
    trgData.extend<unsigned int>("PRESCALE");
    
    unsigned long long& trg_id=trgData["TRG_ID"].data<unsigned long long>();
    unsigned int& trgrunnum=trgData["RUNNUM"].data<unsigned int>();
    unsigned int& cmslsnum=trgData["CMSLSNUM"].data<unsigned int>();
    unsigned int& bitnum=trgData["BITNUM"].data<unsigned int>();
    std::string& bitname=trgData["BITNAME"].data<std::string>();
    unsigned int& count=trgData["TRGCOUNT"].data<unsigned int>();
    unsigned long long& deadtime=trgData["DEADTIME"].data<unsigned long long>();
    unsigned int& prescale=trgData["PRESCALE"].data<unsigned int>();
    
    trglscount=0;
    coral::IBulkOperation* trgInserter=0; 
    unsigned int comittedls=0;
    for(deadIt=deadtimesBeg;deadIt!=deadtimesEnd;++deadIt,++trglscount ){
      unsigned int cmslscount=trglscount+1;
      const BITCOUNT& algoinbits=algocounts[trglscount];
      const BITCOUNT& techinbits=techcounts[trglscount];
      unsigned int trgbitcount=0;
      BITCOUNT::const_iterator algoBitIt;
      BITCOUNT::const_iterator algoBitBeg=algoinbits.begin();
      BITCOUNT::const_iterator algoBitEnd=algoinbits.end();
      if(!lumisession->transaction().isActive()){ 
	lumisession->transaction().start(false);
	coral::ITable& trgtable=lumisession->nominalSchema().tableHandle(LumiNames::trgTableName());
	trgInserter=trgtable.dataEditor().bulkInsert(trgData,2048);
      }else{
	if(deadIt==deadtimesBeg){
	  coral::ITable& trgtable=lumisession->nominalSchema().tableHandle(LumiNames::trgTableName());
	  trgInserter=trgtable.dataEditor().bulkInsert(trgData,2048);
	}
      }
      for(algoBitIt=algoBitBeg;algoBitIt!=algoBitEnd;++algoBitIt,++trgbitcount){
	trg_id = idallocationtable[trglscount].at(trgbitcount);
	deadtime=*deadIt;
	trgrunnum = runnumber;
	cmslsnum = cmslscount;
	bitnum=trgbitcount;
	bitname=algonames[trgbitcount];
	count=*algoBitIt;
	prescale=prescalealgo[cmslscount].at(trgbitcount);
	//std::cout<<"cmslsnum "<<cmslsnum<<" bitnum "<<bitnum<<" bitname "<<bitname<<" prescale "<< prescale<<" count "<<count<<std::endl;
	trgInserter->processNextIteration();	
      }
      BITCOUNT::const_iterator techBitIt;
      BITCOUNT::const_iterator techBitBeg=techinbits.begin();
      BITCOUNT::const_iterator techBitEnd=techinbits.end();
      for(techBitIt=techBitBeg;techBitIt!=techBitEnd;++techBitIt,++trgbitcount){
	trg_id = idallocationtable[trglscount].at(trgbitcount);
	deadtime=*deadIt;
	trgrunnum = runnumber;
	cmslsnum = cmslscount;
	bitnum=trgbitcount;
	bitname=technames[trgbitcount-lumi::N_TRGALGOBIT];
	count=*techBitIt;
	prescale=prescaletech[cmslsnum][trgbitcount-lumi::N_TRGALGOBIT];
	trgInserter->processNextIteration();	
      }
      trgInserter->flush();
      ++comittedls;
      if(comittedls==commitintv){
	std::cout<<"\t committing in LS chunck "<<comittedls<<std::endl; 
	delete trgInserter; trgInserter=0;
	lumisession->transaction().commit();
	comittedls=0;
	std::cout<<"\t committed "<<std::endl; 
      }else if( trglscount==( totalcmsls-1) ){
	std::cout<<"\t committing at the end"<<std::endl; 
	delete trgInserter; trgInserter=0;
	lumisession->transaction().commit();
	std::cout<<"\t done"<<std::endl; 
      }
    }
  }
  void  
  TRG2DB::writeTrgDataToSchema2(coral::ISessionProxy* lumisession,
				unsigned int irunnumber,
				const std::string& source,
			        TriggerDeadCountResult::iterator deadtimesBeg,
				TriggerDeadCountResult::iterator deadtimesEnd,
				TRG2DB::TriggerNameResult_Algo& algonames, 
				TRG2DB::TriggerNameResult_Tech& technames,
				TRG2DB::TriggerCountResult_Algo& algocounts,
				TRG2DB::TriggerCountResult_Tech& techcounts,
				TRG2DB::PrescaleResult_Algo& prescalealgo,
				TRG2DB::PrescaleResult_Tech& prescaletech,
				unsigned int commitintv){
    TRG2DB::TriggerDeadCountResult::iterator deadIt;
    unsigned int totalcmsls=std::distance(deadtimesBeg,deadtimesEnd);
    std::cout<<"inserting totalcmsls "<<totalcmsls<<std::endl;
    coral::AttributeList lstrgData;
    lstrgData.extend<unsigned long long>("DATA_ID");
    lstrgData.extend<unsigned int>("RUNNUM");
    lstrgData.extend<unsigned int>("CMSLSNUM");
    lstrgData.extend<unsigned long long>("DEADTIMECOUNT");
    lstrgData.extend<unsigned int>("BITZEROCOUNT");
    lstrgData.extend<unsigned int>("BITZEROPRESCALE");
    lstrgData.extend<float>("DEADFRAC");
    lstrgData.extend<coral::Blob>("PRESCALEBLOB");
    lstrgData.extend<coral::Blob>("TRGCOUNTBLOB");

    unsigned long long& data_id=lstrgData["DATA_ID"].data<unsigned long long>();
    unsigned int& trgrunnum=lstrgData["RUNNUM"].data<unsigned int>();
    unsigned int& cmslsnum=lstrgData["CMSLSNUM"].data<unsigned int>();
    unsigned long long& deadtime=lstrgData["DEADTIMECOUNT"].data<unsigned long long>();
    unsigned int& bitzerocount=lstrgData["BITZEROCOUNT"].data<unsigned int>();
    unsigned int& bitzeroprescale=lstrgData["BITZEROPRESCALE"].data<unsigned int>();
    float& deadfrac=lstrgData["DEADFRAC"].data<float>();
    coral::Blob& prescaleblob=lstrgData["PRESCALEBLOB"].data<coral::Blob>();
    coral::Blob& trgcountblob=lstrgData["TRGCOUNTBLOB"].data<coral::Blob>();

    unsigned long long branch_id=3;
    std::string branch_name("DATA");
    lumi::RevisionDML revisionDML;
    lumi::RevisionDML::TrgEntry trgrundata;
    std::stringstream op;
    op<<irunnumber;
    std::string runnumberStr=op.str();
    lumisession->transaction().start(false);
    trgrundata.entry_name=runnumberStr;
    trgrundata.source=source;
    trgrundata.runnumber=irunnumber;
    trgrundata.bitzeroname=algonames[0];
    std::string bitnames;
    TriggerNameResult_Algo::iterator bitnameIt;
    TriggerNameResult_Algo::iterator bitnameItBeg=algonames.begin();
    TriggerNameResult_Algo::iterator bitnameItEnd=algonames.end();
    for (bitnameIt=bitnameItBeg;bitnameIt!=bitnameItEnd;++bitnameIt){
      if(bitnameIt!=bitnameItBeg){
	bitnames+=std::string(",");
      }
      bitnames+=*bitnameIt;
    }
    TriggerNameResult_Tech::iterator techbitnameIt;
    TriggerNameResult_Tech::iterator techbitnameItBeg=technames.begin();
    TriggerNameResult_Tech::iterator techbitnameItEnd=technames.end();
    for(techbitnameIt=techbitnameItBeg;techbitnameIt!=techbitnameItEnd;++techbitnameIt){
      bitnames+=std::string(",");
      bitnames+=*techbitnameIt;
    }
    std::cout<<"\tbitnames "<<bitnames<<std::endl;
    trgrundata.bitnames=bitnames;
    trgrundata.entry_id=revisionDML.getEntryInBranchByName(lumisession->nominalSchema(),lumi::LumiNames::trgdataTableName(),runnumberStr,branch_name);
    if(trgrundata.entry_id==0){
      revisionDML.bookNewEntry(lumisession->nominalSchema(),LumiNames::trgdataTableName(),trgrundata);
      std::cout<<"trgrundata revision_id "<<trgrundata.revision_id<<" entry_id "<<trgrundata.entry_id<<" data_id "<<trgrundata.data_id<<std::endl;
      revisionDML.addEntry(lumisession->nominalSchema(),LumiNames::trgdataTableName(),trgrundata,branch_id,branch_name);
  }else{
      revisionDML.bookNewRevision(lumisession->nominalSchema(),LumiNames::trgdataTableName(),trgrundata);
      std::cout<<"trgrundata revision_id "<<trgrundata.revision_id<<" entry_id "<<trgrundata.entry_id<<" data_id "<<trgrundata.data_id<<std::endl;
      revisionDML.addRevision(lumisession->nominalSchema(),LumiNames::trgdataTableName(),trgrundata,branch_id,branch_name);
    }
    std::cout<<"inserting trgrundata "<<std::endl;
    revisionDML.insertTrgRunData(lumisession->nominalSchema(),trgrundata);
    std::cout<<"inserting lstrg data"<<std::endl;
 
    unsigned int trglscount=0;   
    trglscount=0;
    coral::IBulkOperation* lstrgInserter=0; 
    unsigned int comittedls=0;
    for(deadIt=deadtimesBeg;deadIt!=deadtimesEnd;++deadIt,++trglscount ){
      unsigned int cmslscount=trglscount+1;
      if(!lumisession->transaction().isActive()){ 
	lumisession->transaction().start(false);
	coral::ITable& lstrgtable=lumisession->nominalSchema().tableHandle(LumiNames::lstrgTableName());
	lstrgInserter=lstrgtable.dataEditor().bulkInsert(lstrgData,2048);
      }else{
	if(deadIt==deadtimesBeg){
	  coral::ITable& lstrgtable=lumisession->nominalSchema().tableHandle(LumiNames::lstrgTableName());
	  lstrgInserter=lstrgtable.dataEditor().bulkInsert(lstrgData,2048);
	}
      }
      data_id = trgrundata.data_id;
      trgrunnum = irunnumber;
      cmslsnum = cmslscount;
      deadtime = *deadIt;
      bitzerocount = algocounts[trglscount][0];
      bitzeroprescale = prescalealgo[cmslsnum][0];
      if( (bitzerocount*bitzeroprescale )!=0.0){
	deadfrac = deadtime/(bitzerocount*bitzeroprescale);
      }else{
	deadfrac=0.0;
      }
      std::vector<unsigned int> fullprescales;
      fullprescales.reserve(prescalealgo[cmslsnum].size()+prescaletech[cmslsnum].size());
      fullprescales.insert(fullprescales.end(),prescalealgo[cmslsnum].begin(),prescalealgo[cmslsnum].end());
      fullprescales.insert(fullprescales.end(),prescaletech[cmslsnum].begin(),prescaletech[cmslsnum].end());

      prescaleblob.resize(sizeof(unsigned int)*(fullprescales.size()));
      void* prescaleblob_StartAddress = prescaleblob.startingAddress();
      std::memmove(prescaleblob_StartAddress,&fullprescales[0],sizeof(unsigned int)*(fullprescales.size()));
      
      std::vector<unsigned int> fullcounts;
      fullcounts.reserve(algocounts[trglscount].size()+techcounts[trglscount].size());
      fullcounts.insert(fullcounts.end(),algocounts[trglscount].begin(),algocounts[trglscount].end());
      fullcounts.insert(fullcounts.end(),techcounts[trglscount].begin(),techcounts[trglscount].end());
      trgcountblob.resize(sizeof(unsigned int)*(fullcounts.size()));
      void* trgcountblob_StartAddress = trgcountblob.startingAddress();
      std::memmove(trgcountblob_StartAddress,&fullcounts[0],sizeof(unsigned int)*(fullcounts.size()));

      lstrgInserter->processNextIteration();	
      lstrgInserter->flush();
      ++comittedls;
      if(comittedls==commitintv){
	std::cout<<"\t committing in LS chunck "<<comittedls<<std::endl; 
	delete lstrgInserter; lstrgInserter=0;
	lumisession->transaction().commit();
	comittedls=0;
	std::cout<<"\t committed "<<std::endl; 
      }else if( trglscount==( totalcmsls-1) ){
	std::cout<<"\t committing at the end"<<std::endl; 
	delete lstrgInserter; lstrgInserter=0;
	lumisession->transaction().commit();
	std::cout<<"\t done"<<std::endl; 
      }
    }


  }
  const std::string TRG2DB::dataType() const{
    return "TRG";
  }
  const std::string TRG2DB::sourceType() const{
    return "DB";
  }
  //utilities
  std::string TRG2DB::int2str(unsigned int t, unsigned int width){
    std::stringstream ss;
    ss.width(width);
    ss.fill('0');
    ss<<t;
    return ss.str();
  }
  unsigned int TRG2DB::str2int(const std::string& s){
    std::istringstream myStream(s);
    unsigned int i;
    if(myStream>>i){
      return i;
    }else{
      throw lumi::Exception(std::string("str2int error"),"str2int","TRG2DB");
    }
  }
  TRG2DB::~TRG2DB(){}
}//ns lumi
#include "RecoLuminosity/LumiProducer/interface/DataPipeFactory.h"
DEFINE_EDM_PLUGIN(lumi::DataPipeFactory,lumi::TRG2DB,"TRG2DB");
#endif
