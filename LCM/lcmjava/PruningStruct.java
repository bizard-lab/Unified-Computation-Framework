package lcmjava;


/**
 *
 * @author Radek Janoštík <radek.janostik@gmail.com>
 */
public class PruningStruct {
    int[] firstPart;
    int[] markerPart;
    int t;
    
    public PruningStruct(int nAtts) {
        t = 0;
        firstPart = new int[nAtts];
        markerPart=new int[nAtts];
        for (int i=0; i<markerPart.length; i++) {
            markerPart[i] = -1;
        }
    }
    
    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append("LCM_Qtmp: s=0, t=" + t);
        sb.append(", end=" + (firstPart.length*2+3) + ", q:"); // TODO
        for (int i: firstPart) {
            sb.append(i);
            sb.append(", ");
        }
        sb.append("\n");
        for (int i: markerPart) {
            sb.append(i);
            sb.append(", ");
        }
        sb.append("0, 0, "); // TODO
        return  sb.toString();
    }
}
