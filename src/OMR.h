#ifndef OMR_H
#define OMR_H


class OMR
{
public:
    long content = 0L;
    OMR(long c = 0L) {
        this->content = c;
    }

    int bits (int shift, int mask) {
            return (int) ((content >> shift) & mask);
        }

    void setBits(int val, int shift, int mask) {
            val &= mask;
            content = (content & ~ (((long) mask ) << shift))
                               |    ((long) val) << shift;
    }

    static int reverse(unsigned int a) {
        a = ((a >> 1) & 0x55555555) | ((a & 0x55555555) << 1);
        a = ((a >> 2) & 0x33333333) | ((a & 0x33333333) << 2);
        a = ((a >> 4) & 0x0F0F0F0F) | ((a & 0x0F0F0F0F) << 4);
        a = ((a >> 8) & 0x00FF00FF) | ((a & 0x00FF00FF) << 8);
        a = ( a >> 16             ) | ( a               << 16);
        return a;
    }

    int getMode()			{ return bits(0, 7); }
    void setMode(int m)		{ setBits(m, 0, 7); }
    int getCRW_SRW()		{ return bits(3, 1); }
    int getPolarity()		{ return bits(4, 1); }
    void setPolarity(int p)	{ setBits(p, 4, 1); }
    int getPS()				{ return bits(5, 3); }
    int getDisc_CSM_SPM()	{ return bits(7, 1); }
    int getEnable_TP()		{ return bits(8, 1); }
    int getCountL()			{ return bits(9, 3); }
    void setCountL(int c)	{ setBits(c, 9, 3); }
    int getColumnBlock()	{ return bits(11, 7); }
    int getColumnBlockSel()	{ return bits(14, 1); }
    int getRowBlock()		{ return bits(15, 7); }
    int getRowBlockSel()	{ return bits(18, 1); }
    int getEqualization()	{ return bits(19, 1); }
    int getColourMode()		{ return bits(20, 1); }
    int getCSM_SPM()		{ return bits(21, 1); }
    int getInfoHeader()		{ return bits(22, 1); }
    void setInfoHeader(int i){ setBits(i, 22, 1); }
    int getFuseSel()		{ return bits(23, 63); }
    int getFusePulseWidth()	{ return bits(28, 127); }
    int getGainMode()		{ return bits(35, 3); }
    int getSenseDAC()		{ return bits(37, 31); }
    int getExtDAC()			{ return bits(42, 31); }
    int getExtBGSel()		{ return bits(47, 1); }

    int getHighR()          { return reverse(bits(32, 0xffff) << 16); }
    int getLowR()           { return reverse((int) (content)); }

    void setHighR(unsigned int a) { setBits(reverse(a << 16), 32, 0xffff); }
    void setLowR(unsigned int a) { setBits(reverse(a), 0, 0xffffffff); }
};

#endif // OMR_H
