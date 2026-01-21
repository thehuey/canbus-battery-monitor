import React, { useState, useMemo } from 'react';
import { LineChart, Line, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer, ScatterChart, Scatter } from 'recharts';
import { Upload, Info, Battery, Zap } from 'lucide-react';

const CANBusAnalyzer = () => {
  const [rawData, setRawData] = useState(`     index           time           Name             ID        Type      Format  Len      Data
  00000001         786.443.515           RECV            201        DATA    STANDARD    8      2F 3F E4 8F 00 00 00 00 
  00000002         000.000.280           RECV            202        DATA    STANDARD    8      3E CF 00 00 00 00 00 00 
  00000003         000.000.295           RECV            204        DATA    STANDARD    8      22 00 00 00 00 00 00 00 
  00000004         000.000.259           RECV            203        DATA    STANDARD    8      A6 50 00 00 A8 61 00 00`);
  
  const [view, setView] = useState('overview');

  // Parse the CAN data
  const data = useMemo(() => {
    const lines = rawData.trim().split('\n');
    const parsed = [];
    
    for (let i = 1; i < lines.length; i++) {
      const line = lines[i].trim();
      if (!line) continue;
      
      const match = line.match(/(\d+)\s+(\d+\.\d+\.\d+)\s+\w+\s+(\w+)\s+\w+\s+\w+\s+\d+\s+([0-9A-F ]+)/);
      if (match) {
        const [_, index, time, id, dataBytes] = match;
        const bytes = dataBytes.trim().split(/\s+/).map(b => parseInt(b, 16));
        
        parsed.push({
          index: parseInt(index),
          time: parseFloat(time.replace(/\./g, '')),
          id: '0x' + parseInt(id, 16).toString(16).toUpperCase().padStart(3, '0'),
          idNum: parseInt(id, 16),
          bytes: bytes
        });
      }
    }
    
    return parsed;
  }, [rawData]);

  // Analyze all possible 16-bit values for cell voltage range (4000-4200mV)
  const cellVoltageAnalysis = useMemo(() => {
    const findings = [];
    
    data.forEach(msg => {
      for (let i = 0; i < msg.bytes.length - 1; i++) {
        const le = msg.bytes[i] + (msg.bytes[i+1] << 8);
        const be = (msg.bytes[i] << 8) + msg.bytes[i+1];
        
        // Check if values are in cell voltage range
        if (le >= 4000 && le <= 4200) {
          findings.push({
            index: msg.index,
            id: msg.id,
            bytePos: i,
            value: le,
            type: 'LE',
            bytes: `${msg.bytes[i].toString(16).toUpperCase().padStart(2,'0')} ${msg.bytes[i+1].toString(16).toUpperCase().padStart(2,'0')}`
          });
        }
        if (be >= 4000 && be <= 4200) {
          findings.push({
            index: msg.index,
            id: msg.id,
            bytePos: i,
            value: be,
            type: 'BE',
            bytes: `${msg.bytes[i].toString(16).toUpperCase().padStart(2,'0')} ${msg.bytes[i+1].toString(16).toUpperCase().padStart(2,'0')}`
          });
        }
      }
    });
    
    return findings;
  }, [data]);

  // Decode messages by ID
  const decodedMessages = useMemo(() => {
    const byId = {};
    
    data.forEach(msg => {
      if (!byId[msg.id]) byId[msg.id] = [];
      
      const decoded = {
        index: msg.index,
        time: msg.time / 1000000,
        raw: msg.bytes.map(b => b.toString(16).toUpperCase().padStart(2, '0')).join(' '),
      };
      
      // Specific decoding based on ID
      if (msg.id === '0x201') {
        decoded.byte01_le = msg.bytes[0] + (msg.bytes[1] << 8);
        decoded.byte23_le = msg.bytes[2] + (msg.bytes[3] << 8);
        decoded.totalV_hypothesis = decoded.byte23_le / 10; // 36836 / 10 = 3683.6V?
      }
      
      if (msg.id === '0x202') {
        decoded.byte01_le = msg.bytes[0] + (msg.bytes[1] << 8);
        decoded.totalV_mV = decoded.byte01_le; // Raw value
        decoded.avgCellV = decoded.byte01_le / 13; // Divide by 13 cells
        decoded.byte45_le = msg.bytes.length >= 6 ? msg.bytes[4] + (msg.bytes[5] << 8) : 0;
      }
      
      if (msg.id === '0x203') {
        decoded.byte0 = msg.bytes[0];
        decoded.byte0_hex = msg.bytes[0].toString(16).toUpperCase();
        decoded.byte01_le = msg.bytes[0] + (msg.bytes[1] << 8);
        decoded.byte23_le = msg.bytes[2] + (msg.bytes[3] << 8);
        decoded.byte45_le = msg.bytes[4] + (msg.bytes[5] << 8);
        decoded.byte67_le = msg.bytes[6] + (msg.bytes[7] << 8);
      }
      
      if (msg.id === '0x204') {
        decoded.byte0 = msg.bytes[0];
        decoded.byte0_hex = msg.bytes[0].toString(16).toUpperCase();
        decoded.state = decoded.byte0; // Possible state/SoC indicator
      }
      
      byId[msg.id].push(decoded);
    });
    
    return byId;
  }, [data]);

  // Analyze 0x204 for state changes
  const stateAnalysis = useMemo(() => {
    if (!decodedMessages['0x204']) return [];
    
    const states = decodedMessages['0x204'];
    const changes = [];
    let prevState = null;
    
    states.forEach((s, idx) => {
      if (prevState !== null && s.state !== prevState) {
        changes.push({
          index: s.index,
          time: s.time.toFixed(2),
          from: prevState,
          to: s.state,
          fromHex: prevState.toString(16).toUpperCase().padStart(2, '0'),
          toHex: s.state.toString(16).toUpperCase().padStart(2, '0')
        });
      }
      prevState = s.state;
    });
    
    return changes;
  }, [decodedMessages]);

  // Analyze 0x203 byte 0 changes (potential cell index)
  const cellIndexAnalysis = useMemo(() => {
    if (!decodedMessages['0x203']) return [];
    
    const msgs = decodedMessages['0x203'];
    const changes = [];
    let prevByte0 = null;
    
    msgs.forEach((m, idx) => {
      if (prevByte0 !== null && m.byte0 !== prevByte0) {
        changes.push({
          index: m.index,
          time: m.time.toFixed(2),
          from: prevByte0,
          to: m.byte0,
          fromHex: prevByte0.toString(16).toUpperCase().padStart(2, '0'),
          toHex: m.byte0.toString(16).toUpperCase().padStart(2, '0')
        });
      }
      prevByte0 = m.byte0;
    });
    
    return changes;
  }, [decodedMessages]);

  return (
    <div className="w-full min-h-screen bg-gray-50 p-6">
      <div className="max-w-7xl mx-auto">
        <h1 className="text-3xl font-bold text-gray-800 mb-2">🔋 CANBUS Battery Protocol Analyzer</h1>
        <p className="text-gray-600 mb-6">Analyzing 48V 13S 25Ah Li-ion Battery (Tianjin D-power)</p>
        
        {/* View Tabs */}
        <div className="flex space-x-2 mb-6">
          <button
            onClick={() => setView('overview')}
            className={`px-4 py-2 rounded ${view === 'overview' ? 'bg-blue-600 text-white' : 'bg-white text-gray-700'}`}
          >
            Overview
          </button>
          <button
            onClick={() => setView('cellvoltage')}
            className={`px-4 py-2 rounded ${view === 'cellvoltage' ? 'bg-blue-600 text-white' : 'bg-white text-gray-700'}`}
          >
            Cell Voltage Hunt
          </button>
          <button
            onClick={() => setView('commands')}
            className={`px-4 py-2 rounded ${view === 'commands' ? 'bg-blue-600 text-white' : 'bg-white text-gray-700'}`}
          >
            Commands/States
          </button>
          <button
            onClick={() => setView('raw')}
            className={`px-4 py-2 rounded ${view === 'raw' ? 'bg-blue-600 text-white' : 'bg-white text-gray-700'}`}
          >
            Raw Data
          </button>
        </div>

        {/* Overview */}
        {view === 'overview' && (
          <>
            <div className="grid grid-cols-1 md:grid-cols-2 gap-6 mb-6">
              {/* 0x202 Analysis */}
              <div className="bg-white rounded-lg shadow p-6">
                <h2 className="text-xl font-semibold mb-4 flex items-center">
                  <Battery className="w-5 h-5 mr-2" />
                  0x202 - Total Pack Voltage
                </h2>
                {decodedMessages['0x202'] && (
                  <ResponsiveContainer width="100%" height={250}>
                    <LineChart data={decodedMessages['0x202'].slice(0, 100)}>
                      <CartesianGrid strokeDasharray="3 3" />
                      <XAxis dataKey="index" />
                      <YAxis domain={[4000, 4100]} />
                      <Tooltip />
                      <Legend />
                      <Line type="monotone" dataKey="avgCellV" stroke="#8884d8" name="Avg Cell V (÷13)" dot={false} />
                    </LineChart>
                  </ResponsiveContainer>
                )}
                <div className="mt-4 text-sm space-y-2">
                  <p><span className="font-semibold">Analysis:</span> Bytes 0-1 (LE) divided by 13 cells</p>
                  <p><span className="font-semibold">Starting:</span> 53054 / 13 = 4081.8 mV/cell</p>
                  <p><span className="font-semibold">Pattern:</span> Slowly decreasing (charging complete?)</p>
                </div>
              </div>

              {/* 0x204 Analysis */}
              <div className="bg-white rounded-lg shadow p-6">
                <h2 className="text-xl font-semibold mb-4 flex items-center">
                  <Zap className="w-5 h-5 mr-2" />
                  0x204 - State/Command
                </h2>
                {decodedMessages['0x204'] && (
                  <ResponsiveContainer width="100%" height={250}>
                    <LineChart data={decodedMessages['0x204'].slice(0, 100)}>
                      <CartesianGrid strokeDasharray="3 3" />
                      <XAxis dataKey="index" />
                      <YAxis domain={[0, 40]} />
                      <Tooltip />
                      <Legend />
                      <Line type="stepAfter" dataKey="state" stroke="#82ca9d" name="State Value" />
                    </LineChart>
                  </ResponsiveContainer>
                )}
                <div className="mt-4 text-sm space-y-2">
                  <p><span className="font-semibold">Pattern:</span> Countdown from 0x22 → 0x00</p>
                  <p><span className="font-semibold">States:</span> 0x22 (34) → 0x21 → 0x20 → 0x10 (16) → 0x00</p>
                  <p><span className="font-semibold">Hypothesis:</span> Charge state or phase indicator</p>
                </div>
              </div>
            </div>

            {/* Key Findings */}
            <div className="bg-gradient-to-r from-blue-50 to-indigo-50 border border-blue-200 rounded-lg p-6">
              <h2 className="text-xl font-semibold mb-4">🔍 Key Findings</h2>
              <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                <div>
                  <h3 className="font-semibold text-blue-900 mb-2">✅ Decoded Messages:</h3>
                  <ul className="space-y-1 text-sm">
                    <li><span className="font-mono bg-white px-2 py-0.5 rounded">0x201</span> - Constant (possibly pack config)</li>
                    <li><span className="font-mono bg-white px-2 py-0.5 rounded">0x202</span> - Pack voltage (sum of all cells in mV)</li>
                    <li><span className="font-mono bg-white px-2 py-0.5 rounded">0x203</span> - Cell data with incrementing index</li>
                    <li><span className="font-mono bg-white px-2 py-0.5 rounded">0x204</span> - State machine / command</li>
                  </ul>
                </div>
                <div>
                  <h3 className="font-semibold text-blue-900 mb-2">🎯 Charging Indicators:</h3>
                  <ul className="space-y-1 text-sm">
                    <li>0x204 byte 0 changes from 0x22 → 0x00 (countdown)</li>
                    <li>0x202 shows decreasing voltage (end of charge)</li>
                    <li>0x203 byte 0 increments: A6→A7→A8... (cell indexing?)</li>
                  </ul>
                </div>
              </div>
            </div>
          </>
        )}

        {/* Cell Voltage Hunt */}
        {view === 'cellvoltage' && (
          <div className="bg-white rounded-lg shadow p-6">
            <h2 className="text-xl font-semibold mb-4">🔍 Cell Voltage Range Hunt (4000-4200mV)</h2>
            <p className="text-sm text-gray-600 mb-4">
              Searching all 16-bit values (both endianness) that fall in typical Li-ion cell voltage range...
            </p>
            
            {cellVoltageAnalysis.length > 0 ? (
              <div className="overflow-x-auto">
                <table className="min-w-full divide-y divide-gray-200">
                  <thead>
                    <tr>
                      <th className="px-4 py-2 text-left text-xs font-medium text-gray-500 uppercase">Index</th>
                      <th className="px-4 py-2 text-left text-xs font-medium text-gray-500 uppercase">CAN ID</th>
                      <th className="px-4 py-2 text-left text-xs font-medium text-gray-500 uppercase">Byte Pos</th>
                      <th className="px-4 py-2 text-left text-xs font-medium text-gray-500 uppercase">Bytes</th>
                      <th className="px-4 py-2 text-left text-xs font-medium text-gray-500 uppercase">Endian</th>
                      <th className="px-4 py-2 text-left text-xs font-medium text-gray-500 uppercase">Value (mV)</th>
                    </tr>
                  </thead>
                  <tbody className="divide-y divide-gray-200">
                    {cellVoltageAnalysis.slice(0, 50).map((finding, idx) => (
                      <tr key={idx} className="hover:bg-gray-50">
                        <td className="px-4 py-2 text-sm">{finding.index}</td>
                        <td className="px-4 py-2 text-sm font-mono">{finding.id}</td>
                        <td className="px-4 py-2 text-sm">{finding.bytePos}-{finding.bytePos+1}</td>
                        <td className="px-4 py-2 text-sm font-mono">{finding.bytes}</td>
                        <td className="px-4 py-2 text-sm">{finding.type}</td>
                        <td className="px-4 py-2 text-sm font-semibold text-blue-600">{finding.value} mV</td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            ) : (
              <p className="text-gray-500">No values found in 4000-4200mV range. Individual cell voltages may be transmitted differently.</p>
            )}
            
            <div className="mt-6 p-4 bg-yellow-50 border border-yellow-200 rounded">
              <p className="text-sm"><span className="font-semibold">Note:</span> If no individual cell voltages found, the BMS may only transmit total pack voltage (0x202) and not individual cell data on the main CAN interface.</p>
            </div>
          </div>
        )}

        {/* Commands/States */}
        {view === 'commands' && (
          <div className="space-y-6">
            {/* 0x204 State Changes */}
            <div className="bg-white rounded-lg shadow p-6">
              <h2 className="text-xl font-semibold mb-4">0x204 - State Transitions</h2>
              <div className="overflow-x-auto">
                <table className="min-w-full divide-y divide-gray-200">
                  <thead>
                    <tr>
                      <th className="px-4 py-2 text-left text-xs font-medium text-gray-500 uppercase">Index</th>
                      <th className="px-4 py-2 text-left text-xs font-medium text-gray-500 uppercase">Time (s)</th>
                      <th className="px-4 py-2 text-left text-xs font-medium text-gray-500 uppercase">From</th>
                      <th className="px-4 py-2 text-left text-xs font-medium text-gray-500 uppercase">To</th>
                      <th className="px-4 py-2 text-left text-xs font-medium text-gray-500 uppercase">Change</th>
                    </tr>
                  </thead>
                  <tbody className="divide-y divide-gray-200">
                    {stateAnalysis.map((change, idx) => (
                      <tr key={idx} className="hover:bg-gray-50">
                        <td className="px-4 py-2 text-sm">{change.index}</td>
                        <td className="px-4 py-2 text-sm">{change.time}</td>
                        <td className="px-4 py-2 text-sm font-mono">{change.from} (0x{change.fromHex})</td>
                        <td className="px-4 py-2 text-sm font-mono">{change.to} (0x{change.toHex})</td>
                        <td className="px-4 py-2 text-sm">
                          <span className={change.to < change.from ? 'text-red-600' : 'text-green-600'}>
                            {change.to < change.from ? '↓ Decrease' : '↑ Increase'}
                          </span>
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            </div>

            {/* 0x203 Cell Index Changes */}
            <div className="bg-white rounded-lg shadow p-6">
              <h2 className="text-xl font-semibold mb-4">0x203 - Byte 0 Changes (Possible Cell Index)</h2>
              <div className="overflow-x-auto">
                <table className="min-w-full divide-y divide-gray-200">
                  <thead>
                    <tr>
                      <th className="px-4 py-2 text-left text-xs font-medium text-gray-500 uppercase">Index</th>
                      <th className="px-4 py-2 text-left text-xs font-medium text-gray-500 uppercase">Time (s)</th>
                      <th className="px-4 py-2 text-left text-xs font-medium text-gray-500 uppercase">From</th>
                      <th className="px-4 py-2 text-left text-xs font-medium text-gray-500 uppercase">To</th>
                      <th className="px-4 py-2 text-left text-xs font-medium text-gray-500 uppercase">Pattern</th>
                    </tr>
                  </thead>
                  <tbody className="divide-y divide-gray-200">
                    {cellIndexAnalysis.map((change, idx) => (
                      <tr key={idx} className="hover:bg-gray-50">
                        <td className="px-4 py-2 text-sm">{change.index}</td>
                        <td className="px-4 py-2 text-sm">{change.time}</td>
                        <td className="px-4 py-2 text-sm font-mono">0x{change.fromHex} ({change.from})</td>
                        <td className="px-4 py-2 text-sm font-mono">0x{change.toHex} ({change.to})</td>
                        <td className="px-4 py-2 text-sm">
                          {change.to === change.from + 1 ? '✅ Sequential +1' : '⚠️ Non-sequential'}
                        </td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
              <div className="mt-4 p-4 bg-blue-50 rounded">
                <p className="text-sm"><span className="font-semibold">Analysis:</span> Byte 0 increments sequentially (A6→A7→A8...), suggesting this could be a cell index or counter.</p>
              </div>
            </div>
          </div>
        )}

        {/* Raw Data View */}
        {view === 'raw' && (
          <div className="bg-white rounded-lg shadow p-6">
            <h2 className="text-xl font-semibold mb-4">Raw CAN Data</h2>
            <textarea
              value={rawData}
              onChange={(e) => setRawData(e.target.value)}
              className="w-full h-96 font-mono text-xs border rounded p-2"
              placeholder="Paste your CAN data here..."
            />
            <p className="text-sm text-gray-600 mt-2">
              Total messages: {data.length} | Unique IDs: {new Set(data.map(d => d.id)).size}
            </p>
          </div>
        )}
      </div>
    </div>
  );
};

export default CANBusAnalyzer;