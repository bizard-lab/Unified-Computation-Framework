package lcmjava;

import java.util.Comparator;

/**
 *
 * @author Radek Janoštík <radek.janostik@gmail.com>
 */
public class IntRowComparator implements Comparator<int[]> {

    @Override
    public int compare(int[] t, int[] t1) {
        Integer oneC = oneCount(t);
        Integer oneC1 = oneCount(t1);
        if (oneC.equals(oneC1)) {
            for (int i = 0; i < t.length; i++) {
                if (t[i] != t1[i]) {
                    return t[i] - t1[i];
                }
            }
        }
        return oneC1.compareTo(oneC);
    }

    private static int oneCount(int[] array) {
        int sum = 0;
        for (int i : array) {
            if (i == 1) {
                sum++;
            }
        }
        return sum;
    }

}
