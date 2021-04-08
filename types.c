
struct s { int x,y; };
struct p { int x,y; };

int main()
{
    struct s a;
    struct p *b;
    // struct p t = a; fails
    b = (struct p *) &a;
}
