#if !defined(HANDMADE_MATH_H)

/// ==== Vector2 ====
// STRUCT
union Vector2 {
    struct 
    {
        real32 x, y;
    };
    real32 elems[2];
};

// PRIVATE
inline Vector2 operator-(Vector2 a) {
    Vector2 result;
    result.x = -a.x;
    result.y = -a.y;
    return result;
}

inline Vector2 operator+(Vector2 a, Vector2 b) {
    Vector2 result;
    result.x = a.x + b.x;
    result.y = a.y + b.y;
    return result;
}

inline Vector2 operator-(Vector2 a, Vector2 b) {
    Vector2 result;
    result.x = a.x - b.x;
    result.y = a.y - b.y;
    return result;
}

inline Vector2 operator*(Vector2 a, Vector2 b) {
    Vector2 result;
    result.x = a.x * b.x;
    result.y = a.y * b.y;
    return result;
}

inline Vector2 operator*(Vector2 a, real32 scalar) {
    Vector2 result;
    result.x = a.x * scalar;
    result.y = a.y * scalar;
    return result;
}

inline Vector2 operator*(real32 scalar, Vector2 a) {
    Vector2 result;
    result.x = a.x * scalar;
    result.y = a.y * scalar;
    return result;
}

// PUBLIC
inline Vector2& operator+=(Vector2& v, Vector2 other) {
    v = v + other;
    return v;
}

inline Vector2& operator-=(Vector2& v, Vector2 o) {
    v = v - o;
    return v;
}

inline Vector2& operator*=(Vector2& v, real32 scalar) {
    v = v * scalar;
    return v;
}

inline real32 dot_product(Vector2 a, Vector2 b) { // Inner Product
    return a.x * b.x + a.y * b.y;
}

inline real32 length_square(Vector2 a) {
    return dot_product(a, a);
}

// ==== OTHER ====
inline real32 square(real32 value) {
    real32 result = value * value;
    return result;
}

#define HANDMADE_MATH_H
#endif