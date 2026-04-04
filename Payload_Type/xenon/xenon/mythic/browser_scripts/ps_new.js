function(task, responses){
    if(task.status.includes("error")){
        const combined = responses.reduce((prev, cur) => prev + cur, "");
        return {'plaintext': combined};
    } else if(responses.length > 0){
        let rows = [];
        let headers = [
            {"plaintext": "actions",  "type": "button", "cellStyle": {}, "width": 100, "disableSort": true},
            {"plaintext": "ppid",     "type": "number", "copyIcon": true, "cellStyle": {}, "width": 100},
            {"plaintext": "pid",      "type": "number", "copyIcon": true, "cellStyle": {}, "width": 100},
            {"plaintext": "arch",     "type": "string", "cellStyle": {}, "width": 100},
            {"plaintext": "name",     "type": "string", "cellStyle": {}, "fillWidth": true},
            {"plaintext": "user",     "type": "string", "cellStyle": {}, "fillWidth": 250},
            {"plaintext": "session",  "type": "number", "cellStyle": {}, "width": 100},
        ];

        // Known AV/EDR process names - highlighted red
        let avProcesses = ["Tanium","360RP","360SD","360Safe","360leakfixer","360rp","360safe","360sd","360tray","AAWTray","ACAAS","ACAEGMgr","ACAIS","AClntUsr","ALERT","ALERTSVC","ALMon","ALUNotify","ALUpdate","ALsvc","AVENGINE","AVGCHSVX","AVGCSRVX","AVGIDSAgent","AVGIDSMonitor","AVGIDSUI","AVGIDSWatcher","AVGNSX","AVKProxy","AVKService","AVKTray","AVKWCtl","AVP","AVPDTAgt","AcctMgr","Ad-Aware","Ad-Aware2007","AddressExport","AdminServer","Administrator","AeXAgentUIHost","AeXNSAgent","AeXNSRcvSvc","AlertSvc","AluSchedulerSvc","AnVir","AppSvc32","AtrsHost","Auth8021x","AvastSvc","AvastUI","Avconsol","AvpM","Avsynmgr","Avtask","BLACKD","BWMeterConSvc","CAAntiSpyware","CALogDump","CAPPActiveProtection","CB","CCAP","CCenter","CClaw","CLPS","CLPSLA","CLPSLS","CNTAoSMgr","CPntSrv","CTDataLoad","CertificationManagerServiceNT","ClShield","ClamTray","ClamWin","Console","CylanceUI","DAO_Log","DLService","DLTray","DRWAGNTD","DRWAGNUI","DRWEB32W","DRWEBSCD","DRWEBUPW","DRWINST","DSMain","DWHWizrd","DefWatch","DolphinCharge","EHttpSrv","EMET_Agent","EMET_Service","EMLPROUI","EMLPROXY","EMLibUpdateAgentNT","ETConsole3","ETCorrel","ETLogAnalyzer","ETReporter","ETRssFeeds","EUQMonitor","EndPointSecurity","EngineServer","EntityMain","EtScheduler","EtwControlPanel","EventParser","FAMEH32","FCDBLog","FCH32","FPAVServer","FProtTray","FSCUIF","FSHDLL32","FSM32","FSMA32","FSMB32","FWCfg","FireSvc","FireTray","FirewallGUI","ForceField","FortiProxy","FortiTray","FortiWF","FrameworkService","FreeProxy","GDFirewallTray","GDFwSvc","HWAPI","ISNTSysMonitor","ISSVC","ISWMGR","ITMRTSVC","IcePack","IdsInst","InoNmSrv","InoRT","InoRpc","InoTask","InoWeb","IsntSmtp","KABackReport","KANMCMain","KAVFS","KAVStart","KLNAGENT","KMailMon","KNUpdateMain","KPFWSvc","KSWebShield","KVMonXP","KVSrvXP","KWSProd","KWatch","KavAdapterExe","KeyPass","KvXP","LUALL","LWDMServer","LockApp","LockAppHost","LogGetor","MCSHIELD","MCUI32","MSASCui","ManagementAgentNT","McAfeeDataBackup","McEPOC","McEPOCfg","McNASvc","McProxy","McScript_InUse","McWCE","McWCECfg","Mcshield","Mctray","MgntSvc","MpCmdRun","MpfAgent","MpfSrv","MsMpEng","NAIlgpip","NAVAPSVC","NAVAPW32","NCDaemon","NIP","NJeeves","NLClient","NMAGENT","NOD32view","NPFMSG","NPROTECT","NRMENCTB","NSMdtr","NTRtScan","NVCOAS","NVCSched","NavShcom","Navapsvc","NaveCtrl","NaveLog","NaveSP","Navw32","Navwnt","Nip","Njeeves","Npfmsg2","Npfsvice","NscTop","Nvcoas","Nvcsched","Nymse","OLFSNT40","OMSLogManager","ONLINENT","ONLNSVC","OfcPfwSvc","PASystemTray","PAVFNSVR","PAVSRV51","PNmSrv","POPROXY","POProxy","PPClean","PPCtlPriv","PQIBrowser","PSHost","PSIMSVC","PXEMTFTP","PadFSvr","Pagent","Pagentwd","PavBckPT","PavFnSvr","PavPrSrv","PavProt","PavReport","Pavkre","PcCtlCom","PcScnSrv","PccNTMon","PccNTUpd","PpPpWallRun","PrintDevice","ProUtil","PsCtrlS","PsImSvc","PwdFiltHelp","Qoeloader","RAVMOND","RAVXP","RNReport","RPCServ","RSSensor","RTVscan","RapApp","Rav","RavAlert","RavMon","RavMonD","RavService","RavStub","RavTask","RavTray","RavUpdate","RavXP","RealMon","Realmon","RedirSvc","RegMech","ReporterSvc","RouterNT","Rtvscan","SAFeService","SAService","SAVAdminService","SAVFMSESp","SAVMain","SAVScan","SCANMSG","SCANWSCS","SCFManager","SCFService","SCFTray","SDTrayApp","SEVINST","SMEX_ActiveUpdate","SMEX_Master","SMEX_RemoteConf","SMEX_SystemWatch","SMSECtrl","SMSELog","SMSESJM","SMSESp","SMSESrv","SMSETask","SMSEUI","SNAC","SNDMon","SNDSrvc","SPBBCSvc","SPIDERML","SPIDERNT","SSM","SSScheduler","SVCharge","SVDealer","SVFrame","SVTray","SWNETSUP","SavRoam","SavService","SavUI","ScanMailOutLook","SeAnalyzerTool","SemSvc","SescLU","SetupGUIMngr","SiteAdv","Smc","SmcGui","SnHwSrv","SnICheckAdm","SnIcon","SnSrv","SnicheckSrv","SpIDerAgent","SpntSvc","SpyEmergency","SpyEmergencySrv","StOPP","StWatchDog","SymCorpUI","SymSPort","TBMon","TFGui","TFService","TFTray","TFun","TSAnSrf","TSAtiSy","TScutyNT","TSmpNT","TmListen","TmPfw","Tmntsrv","Traflnsp","TrapTrackerMgr","UPSCHD","UcService","UdaterUI","UmxAgent","UmxCfg","UmxFwHlp","UmxPol","Up2date","UpdaterUI","UrlLstCk","UserActivity","UserAnalysis","UsrPrmpt","V3Medic","V3Svc","VPC32","VPDN_LU","VPTray","VSStat","VsStat","VsTskMgr","WEBPROXY","WFXCTL32","WFXMOD32","WFXSNT40","WebProxy","WebScanX","WinRoute","WrSpySetup","ZLH","Zanda","ZhuDongFangYu","Zlh","MSASCuiL","MBAMService","mbamtray","CylanceSvc","cb","MsMpEng","MsSense","CSFalconService","CSFalconContainer","redcloak","OmniAgent","CrAmTray","AmSvc","minionhost","PylumLoader","CrsSvc"];

        // Admin / forensic tools - highlighted cyan
        let adminTools = ["MobaXterm","bash","git-bash","mmc","Code","notepad++","notepad","cmd","drwatson","DRWTSN32","drwtsn32","dumpcap","ethereal","filemon","idag","idaw","k1205","loader32","netmon","netstat","netxray","NmWebService","nukenabber","portmon","powershell","putty","regmon","SystemEye","taskman","TASKMGR","tcpview","Totalcmd","TrafMonitor","windbg","winobj","wireshark","WMonAvNScan","WMonAvScan","WMonSrv","regedit","regedit32","accesschk","accesschk64","AccessEnum","ADExplorer","ADInsight","adrestore","Autologon","Autoruns","Autoruns64","autorunsc","autorunsc64","Bginfo","Bginfo64","Cacheset","Clockres","Clockres64","Contig","Contig64","Coreinfo","ctrl2cap","Dbgview","Desktops","disk2vhd","diskext","diskext64","Diskmon","DiskView","du","du64","efsdump","FindLinks","FindLinks64","handle","handle64","hex2dec","hex2dec64","junction","junction64","ldmdump","Listdlls","Listdlls64","livekd","livekd64","LoadOrd","LoadOrd64","LoadOrdC","LoadOrdC64","logonsessions","logonsessions64","movefile","movefile64","notmyfault","notmyfault64","notmyfaultc","notmyfaultc64","ntfsinfo","ntfsinfo64","pagedfrg","pendmoves","pendmoves64","pipelist","pipelist64","portmon","procdump","procdump64","procexp","procexp64","Procmon","PsExec","PsExec64","psfile","psfile64","PsGetsid","PsGetsid64","PsInfo","PsInfo64","pskill","pskill64","pslist","pslist64","PsLoggedon","PsLoggedon64","psloglist","pspasswd","pspasswd64","psping","psping64","PsService","PsService64","psshutdown","pssuspend","pssuspend64","RAMMap","RegDelNull","RegDelNull64","regjump","ru","ru64","sdelete","sdelete64","ShareEnum","ShellRunas","sigcheck","sigcheck64","streams","streams64","strings","strings64","sync","sync64","Sysmon","Sysmon64","Tcpvcon","Tcpview","Testlimit","Testlimit64","vmmap","Volumeid","Volumeid64","whois","whois64","Winobj","ZoomIt","KeePass","1Password","lastpass"];

        const allText = responses.reduce((prev, cur) => prev + cur, "");
        const lines = allText.split("\n");

        // Xenon output format: name\tppid\tpid[\tarch\tuser\tsession]
        const dataLines = lines.filter(line => line.includes("\t"));

        for(let i = 0; i < dataLines.length; i++){
            const parts = dataLines[i].split("\t");
            const name    = parts[0] || "";
            const ppid    = parts[1] || "";
            const pid     = parts[2] || "";
            const arch    = parts[3] || "";
            const user    = parts[4] || "";
            const session = parts[5] || "";

            // Strip .exe suffix for list lookups - agent includes it, lists don't
            const baseName = name.replace(/\.exe$/i, "");

            let rowStyle = {};
            if(avProcesses.includes(name) || avProcesses.includes(baseName)){
                rowStyle = {backgroundColor: "indianred", color: "black"};
            } else if(adminTools.includes(name) || adminTools.includes(baseName)){
                rowStyle = {backgroundColor: "rgb(106,255,255)", color: "black"};
            } else if(baseName === "explorer" || baseName === "winlogon"){
                rowStyle = {backgroundColor: "cornflowerblue", color: "black"};
            }

            let row = {
                "rowStyle": rowStyle,
                "ppid":    {"plaintext": ppid,    "cellStyle": {}, "copyIcon": true},
                "pid":     {"plaintext": pid,     "cellStyle": {}, "copyIcon": true},
                "arch":    {"plaintext": arch,    "cellStyle": {}},
                "name":    {"plaintext": name,    "cellStyle": {}},
                "user":    {"plaintext": user,    "cellStyle": {}},
                "session": {"plaintext": session, "cellStyle": {}},
                "actions": {"button": {
                    "name": "Actions",
                    "type": "menu",
                    "value": [
                        {
                            "name": "Steal Token",
                            "type": "task",
                            "ui_feature": "steal_token",
                            "parameters": pid
                        },
                        {
                            "name": "Kill",
                            "type": "task",
                            "startIcon": "kill",
                            "ui_feature": "process_browser:kill",
                            "parameters": pid
                        }
                    ]
                }}
            };
            rows.push(row);
        }

        return {"table": [{
            "headers": headers,
            "rows": rows,
        }]};
    } else {
        return {"plaintext": "No response yet from agent..."};
    }
}
