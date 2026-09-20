package com.devexperts.qd.qtp;

import com.devexperts.connector.proto.TransportConnection;
import com.devexperts.io.*;
import com.devexperts.qd.*;
import com.devexperts.qd.kit.*;
import com.devexperts.qd.ng.*;
import com.devexperts.qd.stats.QDStats;
import com.devexperts.util.TypedMap;
import java.lang.reflect.Field;
import java.util.*;

/** Independently authored bounded oracle driver; no upstream implementation copied. */
public class ProtocolOracle {
    static void row(Object... cells) {
        StringJoiner s = new StringJoiner("\t");
        for (Object c : cells) s.add(c == null ? "\\N" : c.toString().replace("\\", "\\\\").replace("\t", "\\t").replace("\n", "\\n").replace("\r", "\\r"));
        System.out.println(s);
    }
    static DefaultRecord record(int id, String name, String... fields) {
        DataIntField[] fs = new DataIntField[fields.length];
        for (int i=0;i<fs.length;i++) fs[i] = new CompactIntField(i, name+"."+fields[i]);
        return new DefaultRecord(id,name,false,fs,new DataObjField[0]);
    }
    static DataScheme scheme(DefaultRecord... records) { return new DefaultScheme(PentaCodec.INSTANCE,records); }
    static void describe(String side, DataScheme s) {
        for(int i=0;i<s.getRecordCount();i++) {
            DataRecord r=s.getRecord(i); row("scheme.record",side,r.getId(),r.getName(),r.hasTime(),r.getMappings().keySet());
            for(int j=0;j<r.getIntFieldCount();j++) { DataField f=r.getIntField(j); row("scheme.field",side,r.getId(),j,f.getName(),f.getLocalName(),f.getPropertyName(),f.getSerialType().getId()); }
        }
    }
    static RecordBuffer buffer(DataScheme s,String name,boolean sub,int... values) {
        RecordBuffer b=new RecordBuffer(sub?RecordMode.SUBSCRIPTION:RecordMode.DATA);
        DataRecord r=s.findRecordByName(name);
        RecordCursor c=b.add(r,s.getCodec().encode("ABC"),null);
        for(int i=0;i<values.length;i++) c.setInt(i,values[i]);
        return b;
    }
    static byte[] compose(DataScheme s, boolean p01) throws Exception {
        ByteArrayOutput out=new ByteArrayOutput(); BinaryQTPComposer c=new BinaryQTPComposer(s,true); c.setOutput(out);
        RecordBuffer b=buffer(s,"R",false,11,22);
        if(p01) { RecordCursor x=b.add(s.findRecordByName("R"),s.getCodec().encode("ABC"),null); x.setInt(0,33); x.setInt(1,44); }
        c.visitData(b,MessageType.STREAM_DATA);
        if(p01) c.visitData(buffer(s,"R",false,55,66),MessageType.STREAM_DATA);
        else c.visitSubscription(buffer(s,"R",true),MessageType.STREAM_ADD_SUBSCRIPTION);
        return out.toByteArray();
    }
    static String hex(byte[] bytes) { StringBuilder s=new StringBuilder(); for(byte b:bytes) s.append(String.format("%02x",b&255)); return s.toString(); }
    // Diagnostic decoding uses upstream primitives; no independent codec claim.
    static void wire(String direction,byte[] bytes) throws Exception {
        row("wire",direction,hex(bytes)); ByteArrayInput in=new ByteArrayInput(bytes);
        while(in.available()>0) {
            long start=in.totalPosition(), length=in.readCompactLong(), end=in.totalPosition()+length;
            int type=length==0?0:in.readCompactInt(); row("frame",direction,start,end,type);
            if(type==2) while(in.totalPosition()<end) {
                int id=in.readCompactInt(); String name=in.readUTFString(); int n=in.readCompactInt(); row("wire.record",direction,id,name,n);
                for(int i=0;i<n;i++) row("wire.field",direction,id,i,in.readUTFString(),in.readCompactInt());
            }
            in.skip(end-in.totalPosition());
        }
    }
    static class Observer extends MessageConsumerAdapter {
        final List<String> records=new ArrayList<>(); int callbacks,errors;
        void take(RecordSource source,MessageType type) {
            int count=0; RecordCursor c;
            while((c=source.next())!=null) {
                DataRecord r=c.getRecord(); StringBuilder v=new StringBuilder(type+"|"+r.getId()+"|"+r.getName()+"|"+r.getScheme().getCodec().decode(c.getCipher(),c.getSymbol()));
                if(type.isData()) for(int i=0;i<r.getIntFieldCount();i++) v.append("|").append(i).append("=").append(c.getInt(i)).append("(").append(r.getIntField(i).toString(c.getInt(i))).append(")");
                String value=v.toString(); records.add(value); row("record",records.size()-1,value); count++;
            }
            row("callback",callbacks++,type,count);
        }
        protected void processData(DataIterator it,MessageType t) { take((RecordSource)it,t); }
        protected void processSubscription(SubscriptionIterator it,MessageType t) { take((RecordSource)it,t); }
        public void handleCorruptedStream(){ errors++; row("error","stream"); }
        public void handleCorruptedMessage(int t){ errors++; row("error","message",t); }
        public void handleUnknownMessage(int t){ errors++; row("error","unknown",t); }
    }
    static List<String> replay(DataScheme s,byte[] bytes,String label,int... chunks) throws Exception {
        row("replay",label,Arrays.toString(chunks)); ChunkedInput in=new ChunkedInput(); BinaryQTPParser p=new BinaryQTPParser(s); p.setInput(in); Observer o=new Observer(); int offset=0;
        for(int n:chunks) {
            in.addAllToInput(ChunkPool.DEFAULT.copyToChunkList(bytes,offset,n,o),o); offset+=n;
            row("parse.begin",offset); p.parse(o); row("parse.end",offset,in.totalPosition(),in.available(),o.records.size(),o.callbacks,o.errors);
        }
        if(offset!=bytes.length || o.errors!=0 || in.available()!=0) throw new AssertionError("incomplete/error "+label);
        return o.records;
    }
    static class Adapter extends MessageAdapter {
        final DataScheme scheme; final Observer observer=new Observer(); RecordBuffer pending;
        Adapter(DataScheme s){super(QDStats.VOID);scheme=s;}
        public DataScheme getScheme(){return scheme;}
        public boolean retrieveMessages(MessageVisitor v){ if(pending!=null){v.visitData(pending,MessageType.STREAM_DATA);pending=null;} return false; }
        protected void processSubscription(SubscriptionIterator i,MessageType t){observer.take((RecordSource)i,t);}
        public void handleCorruptedStream(){observer.handleCorruptedStream();super.handleCorruptedStream();}
        public void handleCorruptedMessage(int t){observer.handleCorruptedMessage(t);super.handleCorruptedMessage(t);}
        public void handleUnknownMessage(int t){observer.handleUnknownMessage(t);super.handleUnknownMessage(t);}
    }
    static class Transport implements TransportConnection {
        final TypedMap vars=new TypedMap(); public TypedMap variables(){return vars;}
        public void markForImmediateRestart(){row("transport","restart");}
        public void connectionClosed(){row("transport","closed");}
        public void chunksAvailable(){row("transport","chunksAvailable");}
        public void readyToProcessChunks(){row("transport","ready");}
    }
    static void p05(String id) throws Exception {
        DataScheme pub=scheme(record(0,"R","x","y")); String[] names=id.endsWith("zero")?new String[0]:id.endsWith("x")?new String[]{"x"}:new String[]{"z"};
        DataScheme peer=scheme(record(0,"S"),record(1,"R",names)); describe("publisher",pub);describe("peer",peer);
        final Adapter adapter=new Adapter(pub);
        MessageAdapterConnectionFactory f=new MessageAdapterConnectionFactory(new MessageAdapter.ConfigurableFactory(){public MessageAdapter createAdapter(QDStats stats){return adapter;}});
        MessageAdapterConnection connection=(MessageAdapterConnection)f.createConnection(new Transport()); connection.start();
        for(String field:new String[]{"nextHeartbeatTime","bytesToNextHeartbeat"}) {Field x=MessageAdapterConnection.class.getDeclaredField(field);x.setAccessible(true);x.setLong(connection,Long.MAX_VALUE);}
        row("instrumentation","heartbeat deadline and byte budget = Long.MAX_VALUE; adapter emits data only; no protocol descriptor/EndpointId; clock marks not serialized");
        row("path",connection.getClass().getName(),"processChunks -> ConnectionQTPParser; retrieveChunks -> ConnectionQTPComposer");
        if(!id.endsWith("none")) {
            ByteArrayOutput out=new ByteArrayOutput(); BinaryQTPComposer c=new BinaryQTPComposer(peer,true);c.setOutput(out);c.visitSubscription(buffer(peer,"R",true),MessageType.STREAM_ADD_SUBSCRIPTION);
            byte[] request=out.toByteArray();wire("peer->publisher",request);row("chunks","request",request.length);
            row("processChunks",connection.processChunks(ChunkPool.DEFAULT.copyToChunkList(request,0,request.length,adapter),adapter));
        } else row("request","absent");
        BinaryRecordDesc requested=connection.getRequestedRecordDesc(pub.getRecord(0));row("requested",requested==null?"absent":requested.isEmpty()?"originally-empty":"nonempty");
        adapter.pending=buffer(pub,"R",false,11,22); ChunkList chunks=connection.retrieveChunks(adapter); ByteArrayOutput out=new ByteArrayOutput();
        if(chunks!=null) {Chunk c;while((c=chunks.poll(adapter))!=null){out.write(c.getBytes(),c.getOffset(),c.getLength());c.recycle(adapter);}chunks.recycle(adapter);}
        byte[] bytes=out.toByteArray();wire("publisher->peer",bytes);
        replay(peer,bytes,"peer-whole",bytes.length);
        replay(scheme(record(0,"R","x","y","z")),bytes,"audit-whole",bytes.length);
        row("connection.state",adapter.isAlive(),adapter.isClosed(),adapter.isMarkedForImmediateRestart(),adapter.observer.errors);connection.close();
    }
    public static void main(String[] args) throws Exception {
        String id=args[0];row("case",id);row("options","describe=true; default protocol options; mixedSubscription=false; event flags/time=0; no field replacer; PentaCodec");
        if(id.startsWith("P05")) p05(id);
        else {
            boolean reverse=id.equals("P03-B-A");
            DataScheme a=reverse?scheme(record(0,"S","x","y"),record(1,"R","x","y")):scheme(record(0,"R",id.equals("P04-alias")?"x.dot":"x","y"),record(1,"S","x","y"));
            DataScheme b;
            if(id.startsWith("P04")) { DefaultRecord r=record(0,"R","y","x","z"); if(id.endsWith("alias")){r.addFieldAlias("x","xdot");row("alias","receiver","x","xdot");row("lookup","xdot",r.findFieldByName("xdot").getIndex());} b=scheme(r); }
            else b=reverse?scheme(record(0,"R","x","y"),record(1,"S","x","y")):scheme(record(0,"S","x","y"),record(1,"R","x","y"));
            describe("sender",a);describe("receiver",b);row("direction",id);
            byte[] bytes=compose(a,id.equals("P01"));wire("sender->receiver",bytes);
            List<String> baseline=replay(b,bytes,"whole",bytes.length);
            if(id.equals("P01")) {
                for(int split=1;split<bytes.length;split++) if(!baseline.equals(replay(b,bytes,"split-"+split,split,bytes.length-split)))throw new AssertionError("semantic split mismatch");
                int[] ones=new int[bytes.length];Arrays.fill(ones,1);if(!baseline.equals(replay(b,bytes,"bytewise",ones)))throw new AssertionError("semantic bytewise mismatch");
                row("semantic.invariance","pass",bytes.length+1,"schedules; callback grouping excluded");
            }
        }
        row("complete",id);
    }
}
