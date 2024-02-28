package lcmjava;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileReader;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.HashSet;
import java.util.LinkedList;
import java.util.List;
import java.util.Set;

/**
 *
 * @author Radek Janoštík <radek.janostik@gmail.com>
 */
public class Transactions {

    int[][] source;
    List<Integer> permutation;
    List<Set<Integer>> attribs;
    List<Integer> counts;
    public int attCount;

    public Transactions() {
        attribs = new LinkedList<>();
        counts = new LinkedList<>();
    }

    public Transactions(int[][] matrix) {

        this(matrix, null);

    }

    public Transactions(String filePath) {
        attribs = new LinkedList<>();
        counts = new LinkedList<>();

        File input = new File(filePath);
        try {
            FileReader fr = new FileReader(input);
            BufferedReader br = new BufferedReader(fr);
            String line;
            int i = 0;
            int maxAtt = Integer.MIN_VALUE;
            while ((line = br.readLine()) != null) {
                attribs.add(new HashSet<>());
                String[] splitted = line.split(",");
                int j = 0;
                for (; j < splitted.length - 1; j++) {
                    int att = Integer.parseInt(splitted[j]);
                    if (att > maxAtt) {
                        maxAtt = att;
                    }
                    attribs.get(i).add(att);
                }

                counts.add(Integer.parseInt(splitted[j].replace(" ", "").replace("(", "").replace(")", "")));
                i++;

            }
            attCount = maxAtt + 1;
            br.close();
            fr.close();

        } catch (Exception e) {
            System.out.println(e);
        }
    }

    public Transactions(int[][] matrix, List<Integer> colPermutation) {
        source = matrix;
        permutation = colPermutation;
        if (colPermutation == null) {
            int[] attribsCount = attribsCount(source);
            permutation = getPermutation(attribsCount);

        }
        attCount = permutation.size();
        Arrays.sort(matrix, new IntRowComparator());
        attribs = new LinkedList<>();
        counts = new LinkedList<>();
        for (int[] row : matrix) {
            Set<Integer> lRow = new HashSet<Integer>();
            int num = 0;
            for (int i : row) {
                if (i == 1) {
                    int permIndex = permutation.indexOf(num);
                    lRow.add(permIndex);
                }
                num++;
            }
            attribs.add(lRow);
            counts.add(1); // TODO duplicity
        }
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < attribs.size(); i++) {

            for (Integer att : attribs.get(i)) {
                sb.append(att);
                sb.append(",");
            }

            sb.append(" (");
            sb.append(counts.get(i));
            sb.append(")\n");
        }
        return sb.toString();
    }

    private static int[] attribsCount(int[][] matrix) {
        int[] res = new int[matrix[0].length];
        for (int i = 0; i < matrix[0].length; i++) {
            for (int[] row : matrix) {
                res[i] = 0;
                if (row[i] == 1) {
                    res[i]++;
                }
            }
        }
        return res;
    }

    private static List<Integer> getPermutation(int[] counts) {
        Integer[] res = new Integer[counts.length];
        fillArrayIndeces(res);
        Comparator<Integer> comparator = (Integer i, Integer j) -> Integer.compare(counts[i], counts[j]);
        Arrays.sort(res, comparator);
        return Arrays.asList(res);
    }

    public void sortBySize() { 
        Integer[] perm = new Integer[attribs.size()];
        fillArrayIndeces(perm);
        Arrays.sort(perm, new Comparator<Integer>() {
            @Override
            public int compare(Integer t, Integer t1) {
                Set<Integer> first = attribs.get(t);
                Set<Integer> second = attribs.get(t1);
                Integer firstSize = first.size();
                Integer secondSize = second.size();
                return firstSize.compareTo(secondSize) * (-1);
            }
        });
        List<Set<Integer>> nextAttribs = new ArrayList<>(attribs.size());
        List<Integer> nextCounts = new ArrayList<>(attribs.size());
        int pos = 0;
        for (int i : perm) {
            nextAttribs.add(pos, attribs.get(i));
            nextCounts.add(pos, counts.get(i));
            pos++;
        }
        attribs = nextAttribs;
        counts = nextCounts;
    }

    private static void fillArrayIndeces(Integer[] array) {
        for (int i = 0; i < array.length; i++) {
            array[i] = i;
        }
    }

}
