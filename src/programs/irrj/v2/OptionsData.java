import javax.swing.*;

public class OptionsData {
    JTextField qs, qp, ss, sp, mp, pp, pu, pe, pr;
    public String qss, qps, sss, sps, mps, pps, pus, pes, prs;
    boolean boxState[];

    public OptionsData(JTextField queryServer, JTextField queryPort, JTextField submitServer,
		       JTextField submitPort,  JTextField mpass,     JTextField ppass,
		       JTextField pguid,       JTextField pgexe,     JTextField pgrin,
		       ListModel  lm){
	qs = queryServer;
	qp = queryPort;
	ss = submitServer;
	sp = submitPort;
	mp = mpass;
	pp = ppass;
	pu = pguid;
	pe = pgexe;
	pr = pgrin;
	save(lm);
    }

    public boolean save(ListModel in){
	boolean changeServer = false;
	if(qss != null && qps != null){
	    if(!qss.equals(qs.getText().trim()) ||
	       !qps.equals(qp.getText().trim()) )
		changeServer = true;
	}

	qss = qs.getText().trim();
	qps = qp.getText().trim();
	sss = ss.getText().trim();
	sps = sp.getText().trim();
	mps = mp.getText().trim();
	pps = pp.getText().trim();
	pus = pu.getText().trim();
	pes = pe.getText().trim();
	prs = pr.getText().trim();
	
	if(changeServer){
	    IRRj.gr.changeServer(qss, Integer.valueOf(qps).intValue());
	    return true;
	}

	int numItems = in.getSize(); 
	boxState = new boolean[numItems];
	StringBuffer dbs = new StringBuffer();
	for(int i = 0; i < numItems; i++){
	    boxState[i] = ((JCheckBox)in.getElementAt(i)).isSelected();
	    if(boxState[i]){
		dbs.append(((JCheckBox)in.getElementAt(i)).getText().trim());
		dbs.append(",");
	    }
	}
	
	IRRj.gr.getNewRequest("!s" + dbs.toString());
	return false;
    }

    public void revert(ListModel in){
	qs.setText(qss);
	qp.setText(qps);
	ss.setText(sss);
	sp.setText(sps);
	mp.setText(mps);
	pp.setText(pps);
	pu.setText(pus);
	pe.setText(pes);
	pr.setText(prs);
	int numItems = boxState.length;
	for(int i = 0; i < numItems; i++)
	    ((JCheckBox)in.getElementAt(i)).setSelected(boxState[i]);
    }
}
