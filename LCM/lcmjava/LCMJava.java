package lcmjava;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.PrintStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.LinkedHashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;
import java.util.TreeSet;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.logging.Level;
import java.util.logging.Logger;

/**
 *
 * @author Radek Janoštík <radek.janostik@gmail.com>
 */
public class LCMJava {

    public static final int THRESHOLD = 1;

    /**
     * @param args the command line arguments
     */
    public static void main(String[] args) throws FileNotFoundException {
        if (args.length == 0) {
//            PrintStream out = new PrintStream(new FileOutputStream("/home/radek/upol/phd/dev/lcmJava.txt"));
//            System.setOut(out);
//        int[][] matrix = {{1, 0, 1, 1},
//        {1, 0, 0, 1},
//        {0,0,1,0},
//        {1, 0, 0, 0,}
//    };
//        int[][] matrix
//                = {{0, 1, 1, 1, 0, 0, 1},
//                {1, 1, 0, 1, 1, 1, 1},
//                {0, 1, 0, 1, 0, 1, 0},
//                {0, 0, 1, 1, 1, 0, 1},
//                {1, 0, 1, 0, 1, 1, 1},
//                {0, 0, 0, 0, 1, 1, 0}};
//        Transactions t = new Transactions(matrix, Arrays.asList(new Integer[]{3, 4, 5, 6, 1, 2, 0}));
            int[][] matrix = readFimiMatrix("/home/radek/upol/phd/datasets/mushroom.dat");
            Transactions t = new Transactions(matrix, Arrays.asList(new Integer[]{85, 86, 34, 90, 36, 39, 59, 63, 24, 53, 67, 76, 2, 110, 93, 1, 56, 3, 28, 52, 23, 10, 6, 116, 94, 9, 38, 58, 102, 61, 11, 66, 13, 29, 114, 99, 69, 77, 98, 16, 48, 111, 101, 17, 43, 37, 95, 107, 44, 117, 54, 14, 41, 15, 119, 7, 42, 45, 64, 91, 31, 32, 68, 78, 55, 60, 80, 46, 4, 70, 71, 79, 40, 26, 27, 108, 109, 113, 112, 115, 65, 25, 35, 30, 57, 73, 83, 118, 19, 18, 47, 72, 81, 87, 88, 50, 103, 51, 96, 100, 104, 105, 106, 21, 33, 74, 84, 92, 97, 5, 49, 62, 82, 20, 22, 75, 89, 8, 12}));
//        Transactions t = new Transactions("/home/radek/upol/phd/dev/LCMJava/trsacts_mushroom1000.txt");
            Occurencies occ = new Occurencies(t);
            PruningStruct ps = new PruningStruct(t.attCount);
            LCMclosed(t, occ, ps);
        }
    }

    public static int[][] readFimiMatrix(String filePath) {
        File input = new File(filePath);
        try {
            FileReader fr = new FileReader(input);
            BufferedReader br = new BufferedReader(fr);
            String line;
            List<List<Integer>> lines = new LinkedList<>();
            int i = 0;
            int maxAtt = Integer.MIN_VALUE;
            while ((line = br.readLine()) != null) {
                lines.add(new LinkedList<>());
                for (String s : line.split(" ")) {
                    int att = Integer.parseInt(s);
                    if (att > maxAtt) {
                        maxAtt = att;
                    }
                    lines.get(i).add(att);
                }
                i++;

            }
            br.close();
            fr.close();
            int[][] result = new int[lines.size()][maxAtt + 1];
            for (i = 0; i < lines.size(); i++) {
                for (Integer att : lines.get(i)) {
                    result[i][att] = 1;
                }
            }
            return result;
        } catch (Exception e) {
        }
        return new int[0][0];
    }

    public static void LCMclosed(Transactions t, Occurencies occ, PruningStruct ps) {
        for (int i = 0; i < t.attCount; i++) {
            occ.s.add(i, occ.end.get(i));
            occ.end.set(i, 0);
        }
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < t.attCount; i++) {
            LCMclosedIter(t, i, -1, occ, new HashSet<Integer>(), ps, sb);

        }
         System.out.print(sb.toString());
        System.out.print("realEnd");

    }

    public static String HashSetToString(Set<Integer> set) {
        return HashSetToString(set, false);
    }

    public static String HashSetToString(Set<Integer> set, boolean reverse) {
        StringBuilder sb = new StringBuilder();
        List<Integer> res = new ArrayList<>(set.size());
        for (Integer i : set) {
            res.add(i);
        }
        if (reverse) {
            Collections.reverse(res);
        }
        for (Integer i : res) {
            sb.append(i);
            sb.append(",");
        }
        return sb.toString();
    }

    public static int LCMclosedIter(Transactions t, int item, int prevItem, Occurencies occ, Set<Integer> currentItemset, PruningStruct ps, StringBuilder sb) {
        currentItemset.add(item);
        int qt = ps.t;

        Set<Integer> jump = new TreeSet<>();

        LCMFreqCalc(t, item, occ, jump);

        if (currentItemset.size() > 1) {                                            // pokud jsme hloubeji v rekurzi, vynulujeme .end predchoziho
            occ.end.set(prevItem, 0); // 135
        }
        occ.end.set(item, 0); // 136                                                // vynulujeme .end soucasneho

        int ii = LCMJumpRmInfrequent(occ, jump, occ.s.get(item), currentItemset);

        if (ii > item) {                                                            // vlozili-li jsme neco vetsiho nez soucasny
            cleanEnd(occ, jump);                                                    // uklidime
            return ii;                                                              // Vratime vlozeny
        }

        LCMPrintSolution(currentItemset, sb);                                           // Jinak tiskneme

        // QUEUE LOOP
        int count = 0;                                                              // Najdeme nejvetsi z jumpu mensi nez soucasny item
        int ee = -1;
        for (int j : jump) {
            if (j < item) {
                count++;
                if (j > ee) {
                    ee = j;
                }
            }
        }

        if (count == 0) {                                                           // Zadny neni mensi
            cleanEnd(occ, jump);                                                    // Uklidime        
            return -1;                                                              // Skoncime
        }
        occ.end.set(item, THRESHOLD); // line 157                                   // Nastaveni occ.end soucasneho na threshold (PROC?)

        Transactions tt = LCMMkFreqTrsact(t, occ, item, ee);                        // Zredukujeme transakce na ty, obshujici attributy <= ee, odstranene atributy s occ.end <= 0

        if (count >= 2 && tt.counts.size() > 5) {                                   // Pokud je tam tech mensi dostatek (count) a zaroven je db "dost velka" shrinkujeme
            tt = LCMShrink(tt, item);                                               // Transakce shodne na restrikci <=item zprunikuje
        }

        OccDeliverAll(tt, occ, item);                                               // Prepocitani okurenci podle nove databaze (pro itemy < maxItem

        LCMRemoveLargeItems(jump, item, occ);                                       // Odstrani z jumpu vetsi nez item, vynuluje vsem occ.end, u mensich si preuzloti occ.s<-occ.end

        for (int i : jump) {                                                        // Projdeme vsechny prvky, ktere zbyly v jumpu a u nekterych rekurzivne zavolame

            LCMPruningDel(qt, i, ps);

            if (ps.markerPart[i] == -1) {

                ii = LCMclosedIter(tt, i, item, occ, new LinkedHashSet<Integer>(currentItemset), ps, sb);       //Rekurzivni volani na mensi db, i z jumpu, previous<-item, kopie currentItemsetu

                LCMPruningSet(i, ii, qt, ps);

            } else {
//                System.out.println("Skipped i=" + i + ", current itemset: " + currentItemset);
            }
            occ.tCounts[i] = 0;
            occ.end.set(i, 0);
        }

        LCMPruningDel(qt, t.attCount, ps);

        return -1;

    }

    /**
     * Uklid (v c navesti END2)
     *
     * @param occ
     * @param jump
     */
    public static void cleanEnd(Occurencies occ, Set<Integer> jump) {
        cleanOccurencies(occ, jump);
    }

    /**
     * Vytvori nove transakce, kde zprunikuje ty transakce, kt jsou shodne na
     * restrikci mensi rovno maxItem Seradi je podle velikosti
     *
     * param tt -- soucasne transakce
     *
     * @param maxItem
     * @return
     */
    public static Transactions LCMShrink(Transactions tt, int maxItem) {
        HashMap<Set, List<Set>> map = new HashMap<>();
        HashMap<Set, Integer> counts = new HashMap<>();
        for (int t = 0; t < tt.attribs.size(); t++) {                                // Projdeme transakce
            Set<Integer> cut = tCut(tt.attribs.get(t), maxItem);                    // Orez na <= maxitem
            if (!map.keySet().contains(cut)) {
                map.put(cut, new LinkedList<>());
                counts.put(cut, 0);
            }
            map.get(cut).add(tt.attribs.get(t));                                    // Pridame do slovniku
            counts.put(cut, counts.get(cut) + tt.counts.get(t));                    // Pricteme pocet vyskytu
        }
        Transactions shrinked = new Transactions();
        shrinked.attCount = tt.attCount;
        int shrinkedI = 0;
        for (Set key : map.keySet()) {                                              // Vypocet pruniku, kterych se shoduji na orezu
            Set first = map.get(key).get(0);
            for (int i = 1; i < map.get(key).size(); i++) {
                first.retainAll(map.get(key).get(i));
            }
            shrinked.attribs.add(shrinkedI, first);                                 // Pridani do nove databasea
            shrinked.counts.add(shrinkedI, counts.get(key));
            shrinkedI++;
        }
        shrinked.sortBySize();                                                      // Sort podle velikosti
        return shrinked;
    }

    /**
     *
     * @param set
     * @param maxItem
     * @return Orez mnoziny set na prvky <= maxitem
     */
    public static Set<Integer> tCut(Set<Integer> set, int maxItem) {
        Set<Integer> result = new HashSet<>();
        for (Integer i : set) {
            if (i <= maxItem) {
                result.add(i);
            }
        }
        return result;
    }

    public static void LCMPruningDel(int tail, int upperTh, PruningStruct ps) {
        while (tail < ps.t) {                                                       // dokud se nevrátíme na původní velikost
            if (ps.markerPart[ps.firstPart[ps.t - 1]] > upperTh) {
                break;
            }
            ps.t -= 1;                                                              // snížíme počet
            ps.markerPart[ps.firstPart[ps.t]] = -1;                                 // nastavíme marker podle první části
        }
    }

    public static void LCMPruningSet(int item, int markItem, int qt, PruningStruct ps) {
        int lastT = ps.t;
        if (markItem >= 0) {
            ps.t += 1;
            while (qt < lastT) {
                if (ps.markerPart[ps.firstPart[lastT - 1]] >= markItem) {
                    break;
                }
                ps.firstPart[lastT] = ps.firstPart[lastT - 1];
                lastT--;
            }
            ps.markerPart[item] = markItem;
            ps.firstPart[lastT] = item;
        }

    }

    /**
     * 1) Odstrani z jumpu ty j, vetsi nez item 2) Mensim nastavi occ.s
     * <-occ.end 3) Vsem vynuluje occ.end @param jump @param item
     *
     * @
     * p
     * aram occ
     */
    public static void LCMRemoveLargeItems(Set<Integer> jump, int item, Occurencies occ) {
        List<Integer> toRemove = new LinkedList<>();
        for (int j : jump) {
            // 1) remove greater than item
            if (j > item) {
                toRemove.add(j);
            } else if (j < item) {
                // 2) set occ[e].s = occ[e].end for each i<max_item (preserve frequency)
                occ.s.set(j, occ.end.get(j));
            }
            // 3) set occ[i].end = 0 for each i in jump
            occ.end.set(j, 0);

        }
        jump.removeAll(toRemove);
        // 4) sort in increasing                                                    // Je v komentari v puvodnim kodu, ale nedela se
    }

    /**
     * Pro itemy mensi nez maxItem prepocita vyskyty transakci
     *
     * @param t
     * @param occ
     * @param maxItem
     */
    public static void OccDeliverAll(Transactions t, Occurencies occ, int maxItem) {
        for (int item = 0; item < maxItem; item++) {                                // Pro itemy mensi nez maxItem upravime transakce
            occ.transactions.get(item).clear();                                     // Nejprve vymazeme puvodni
            for (int tIndex = 0; tIndex < t.attribs.size(); tIndex++) {             // Pak pridame ty, ktere jej obsahuji
                if (t.attribs.get(tIndex).contains(item)) {
                    occ.transactions.get(item).add(tIndex);
                }
            }
            occ.tCounts[item] = occ.transactions.get(item).size();                  // Nastaveni korektniho poctu
        }
    }

    /**
     * Vytvori nove transakce obsahujici nejaky att mensi rovno maxLesser Z nich
     * vybere pouze ty atributy majici occ.end > 0
     *
     * @param t -- puvodni transakce
     * @param occ -- okurence
     * @param item -- soucasny item
     * @param maxLesser -- nejvetsi mensi
     * @return -- Nove transakce
     */
    public static Transactions LCMMkFreqTrsact(Transactions t, Occurencies occ, int item, int maxLesser) {
        Transactions res = new Transactions();
        res.attCount = t.attCount;
        int tPos = 0;
        for (int tIndex : occ.transactions.get(item)) {                             // Projdeme transakce itemu
            boolean canAdd = false;
            for (int i = 0; i <= maxLesser; i++) {                                  // Musi obsahovat nejaky mensi rovno maxLesser
                if (t.attribs.get(tIndex).contains(i)) {
                    canAdd = true;
                    break;
                }
            }

            if (canAdd) {                                                           // Pokud muzeme pridat 
                // pouze s okurencema
                Set<Integer> attribs = new HashSet<>();                             // Nova transakce
                for (int attrib : t.attribs.get(tIndex)) {
                    if (occ.end.get(attrib) > 0) {                                  // Naplnena puvodnimi atributy s occ.end > 0
                        attribs.add(attrib);
                    }
                }
                res.attribs.add(tPos, attribs);                                     // Pridani nove transakce
                res.counts.add(tPos, t.counts.get(tIndex));
                tPos++;
            }
        }
        return res;
    }

    /**
     * Tisk reseni
     *
     * @param itemset
     */
    public static void LCMPrintSolution(Set<Integer> itemset, StringBuilder sb) {
        sb.append(HashSetToString(itemset));
        sb.append("\n");
    }

    /**
     * Vsem z jumpu vynuluje occ.end (uklid)
     *
     * @param occ
     * @param jump
     */
    public static void cleanOccurencies(Occurencies occ, Set<Integer> jump) {
        for (int j : jump) {
            occ.end.set(j, 0);
        }
    }

    /**
     * 1) Odstrani z jumpu prvky, ktere maji mensi pocet vyskytu nez threshold
     * 2) Do soucesneho itemsetu vlozi ty itemy, ktere maji stejny pocet vyskytu
     * jako soucasny 2 a) Ty stejne odstrani i z jumpu 2 b) nastavi jim occ.end
     * = 0 3) Vrati nejvyssi vlozeny
     *
     * @param occ -- okurence
     * @param jump -- jump
     * @param currFreq -- puvodni(?) frekvence soucasneho
     * @param currItemset -- soucasny itemset (kam se bude pripadne pridavat)
     * @return Nejvyssi prvek vlozeny do currItemset
     */
    public static int LCMJumpRmInfrequent(Occurencies occ, Set<Integer> jump, int currFreq, Set<Integer> currItemset) {
        List<Integer> toRemove = new LinkedList<>();
        int max = -1;
        for (Integer j : jump) {                                                    // Projdeme jump
            // 1) remove infrequent from Jump           
            if (occ.end.get(j) < THRESHOLD) {
                toRemove.add(j);                                                    // Itemy pod thresholdem pripravime k odstraneni  (1)
            } else if (occ.end.get(j) == currFreq) {                                // Itemy se stejnou frekvenci
                // 2) insert i with frq(i) = currFreq 
                if (j > max) {
                    max = j;                                                        // Zapamatujeme si nejvetsi (3)
                }
                currItemset.add(j);                                                 // Pridame do soucasneho itemsetu (2)
                toRemove.add(j);                                                    // Pripravime k ostraneni (2a)
                occ.end.set(j, 0); // nezdokumentovane nastaveni na 0               // Vynulujeme jejich occ.end (2b)
            }
        }
        jump.removeAll(toRemove);
        // 3 ) return maximum i st. frq(i) = curr
        return max;
    }

    /**
     * 1) Naplni jump atributy vznikle sjednocenim transakci, ve kterych se
     * vyskytuje item
     *
     * 2) Pro atributy z jumpu nastavi occ.end na pocet transakci, ve kterych se
     * vyskytuji
     *
     * @param t -- soucasne transakce
     * @param item -- soucasny item
     * @param occ -- soucasne okurence
     * @param jump -- kam se naplni itemy
     */
    public static void LCMFreqCalc(Transactions t, int item, Occurencies occ, Set<Integer> jump) {
        for (Integer tIndex : occ.transactions.get(item)) {                         // iterace pres transakce itemu
            int tCount = t.counts.get(tIndex);
            for (Integer tAtt : t.attribs.get(tIndex)) {                            // iterace pres itemy transakci
                if (jump.size() < t.attCount) {
                    jump.add(tAtt);                                                 // sjednoceni transakci (1)
                }
                occ.end.set(tAtt, occ.end.get(tAtt) + tCount);                      // k okurenci.end atributu pricte pocet vyskytu dane transakce (2)
            }
        }
    }

}
