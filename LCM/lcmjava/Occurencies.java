package lcmjava;

import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.TreeSet;

/**
 *
 * @author Radek Janoštík <radek.janostik@gmail.com>
 */
public class Occurencies {

    List<Integer> s;
    int[] tCounts;
    List<Integer> end;
    List<TreeSet<Integer>> transactions;

    public Occurencies(Transactions t) {
        s = new ArrayList<>(t.attCount);
        tCounts = new int[t.attCount];
        end = new ArrayList<>(t.attCount);
        transactions = new ArrayList<>(t.attCount);
        
        for (int i = 0; i < t.attCount; i++) {
            transactions.add(new TreeSet<Integer>());
        }
        for (int i = 0; i < t.attribs.size(); i++) {
            for (Integer att : t.attribs.get(i)) {
                tCounts[att] =  tCounts[att] + t.counts.get(i);
                transactions.get(att).add(i);
            }
        }
        for (int i : tCounts) { //copy
            end.add(i);
        }
    }
    
    public Occurencies(Occurencies occ) {
        s = new ArrayList<>(occ.s);
        tCounts = occ.tCounts.clone();
        end = new ArrayList<>(occ.end);
        transactions = new ArrayList<>();
        for (TreeSet<Integer> t: occ.transactions) {
            transactions.add((TreeSet<Integer>)t.clone());
        }
    }
    
    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append("Okurence:\n");
        for (int i = 0; i < tCounts.length; i++) {
            sb.append("attribut " + i + ": pocet transakci: ");
            sb.append(tCounts[i]);
            sb.append(", end: ");
            sb.append(end.get(i));
            sb.append(", transakce: ");
            if (tCounts[i] > 0) {
                for (Integer t : transactions.get(i)) {
                    sb.append(t);
                    sb.append(",");
                }
            }
            sb.append("\n");
        }
        return sb.toString();
    }
}
