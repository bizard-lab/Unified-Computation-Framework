package lcmjava;

import java.util.LinkedHashSet;
import java.util.Set;
import static lcmjava.LCMJava.LCMclosedIter;

/**
 *
 * @author Radek Janoštík <radek.janostik@gmail.com>
 */
public class Task implements Runnable {

        Occurencies nextOcc;
        Set<Integer> results;
        PruningStruct nextPs;
        Transactions t;
        int i;

        public Task(Occurencies occ, PruningStruct ps, Transactions t, int i) {
            nextOcc = new Occurencies(occ);
            results = new LinkedHashSet<>();
            nextPs = new PruningStruct(ps.firstPart.length);
            this.t = t;
            this.i = i;
        }

        // Prints task name and sleeps for 1s 
        // This Whole process is repeated 5 times 
        public void run() {
            StringBuilder sb = new StringBuilder();
            LCMclosedIter(t, i, -1, nextOcc, results, nextPs,sb);
//            System.out.println(sb.toString());
        }
    }