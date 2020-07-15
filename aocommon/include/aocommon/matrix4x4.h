#ifndef MATRIX_4X4_H
#define MATRIX_4X4_H

#include <complex>
#include <string>
#include <sstream>
#include <stdexcept>

#include <aocommon/matrix2x2.h>

class Matrix4x4 {
 public:
  Matrix4x4() {}

  Matrix4x4(std::initializer_list<std::complex<double>> list) {
    if (list.size() != 16)
      throw std::runtime_error(
          "Matrix4x4 needs to be initialized with 16 items");
    size_t index = 0;
    for (const std::complex<double>& el : list) {
      _data[index] = el;
      ++index;
    }
  }

  static Matrix4x4 Zero() { return Matrix4x4(); }

  static Matrix4x4 Unit() {
    Matrix4x4 unit;
    unit[0] = 1.0;
    unit[5] = 1.0;
    unit[10] = 1.0;
    unit[15] = 1.0;
    return unit;
  }

  Matrix4x4 operator+(const Matrix4x4& rhs) const {
    Matrix4x4 result;
    for (size_t i = 0; i != 16; ++i) result[i] = this->_data[i] + rhs._data[i];
    return result;
  }

  Matrix4x4& operator+=(const Matrix4x4& rhs) {
    for (size_t i = 0; i != 16; ++i) _data[i] += rhs._data[i];
    return *this;
  }

  Matrix4x4 operator*(const std::complex<double>& rhs) const {
    Matrix4x4 m;
    for (size_t i = 0; i != 16; ++i) m[i] = _data[i] * rhs;
    return m;
  }

  Vector4 operator*(const Vector4& rhs) const {
    Vector4 v(_data[0] * rhs[0], _data[4] * rhs[0], _data[8] * rhs[0],
              _data[12] * rhs[0]);
    for (size_t i = 1; i != 4; ++i) {
      v[0] += _data[i] * rhs[i];
      v[1] += _data[i + 4] * rhs[i];
      v[2] += _data[i + 8] * rhs[i];
      v[3] += _data[i + 12] * rhs[i];
    }
    return v;
  }

  bool Invert() {
    std::complex<double> inv[16];
    const std::complex<double>* m = _data;

    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] +
             m[9] * m[7] * m[14] + m[13] * m[6] * m[11] - m[13] * m[7] * m[10];

    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] +
             m[8] * m[6] * m[15] - m[8] * m[7] * m[14] - m[12] * m[6] * m[11] +
             m[12] * m[7] * m[10];

    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] +
             m[8] * m[7] * m[13] + m[12] * m[5] * m[11] - m[12] * m[7] * m[9];

    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] +
              m[8] * m[5] * m[14] - m[8] * m[6] * m[13] - m[12] * m[5] * m[10] +
              m[12] * m[6] * m[9];

    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] +
             m[9] * m[2] * m[15] - m[9] * m[3] * m[14] - m[13] * m[2] * m[11] +
             m[13] * m[3] * m[10];

    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] +
             m[8] * m[3] * m[14] + m[12] * m[2] * m[11] - m[12] * m[3] * m[10];

    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] -
             m[8] * m[3] * m[13] - m[12] * m[1] * m[11] + m[12] * m[3] * m[9];

    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] +
              m[8] * m[2] * m[13] + m[12] * m[1] * m[10] - m[12] * m[2] * m[9];

    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] +
             m[5] * m[3] * m[14] + m[13] * m[2] * m[7] - m[13] * m[3] * m[6];

    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] -
             m[4] * m[3] * m[14] - m[12] * m[2] * m[7] + m[12] * m[3] * m[6];

    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] +
              m[4] * m[3] * m[13] + m[12] * m[1] * m[7] - m[12] * m[3] * m[5];

    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] -
              m[4] * m[2] * m[13] - m[12] * m[1] * m[6] + m[12] * m[2] * m[5];

    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] -
             m[5] * m[3] * m[10] - m[9] * m[2] * m[7] + m[9] * m[3] * m[6];

    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] +
             m[4] * m[3] * m[10] + m[8] * m[2] * m[7] - m[8] * m[3] * m[6];

    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] -
              m[4] * m[3] * m[9] - m[8] * m[1] * m[7] + m[8] * m[3] * m[5];

    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] +
              m[4] * m[2] * m[9] + m[8] * m[1] * m[6] - m[8] * m[2] * m[5];

    std::complex<double> det =
        m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

    if (det == 0.0) return false;

    det = 1.0 / det;

    for (size_t i = 0; i < 16; i++) _data[i] = inv[i] * det;

    return true;
  }

  std::complex<double>& operator[](size_t i) { return _data[i]; }

  const std::complex<double>& operator[](size_t i) const { return _data[i]; }

  double Norm() const {
    double n = 0.0;
    for (size_t i = 0; i != 16; ++i) {
      n += std::norm(_data[i]);
    }
    return n;
  }

  std::string String() const {
    std::ostringstream str;
    for (size_t y = 0; y != 4; ++y) {
      for (size_t x = 0; x != 3; ++x) {
        str << _data[x + y * 4] << '\t';
      }
      str << _data[3 + y * 4] << '\n';
    }
    return str.str();
  }

  static Matrix4x4 KroneckerProduct(const MC2x2& veca, const MC2x2& vecb) {
    Matrix4x4 result;
    const size_t posa[4] = {0, 2, 8, 10};
    for (size_t i = 0; i != 4; ++i) {
      result[posa[i]] = veca[i] * vecb[0];
      result[posa[i] + 1] = veca[i] * vecb[1];
      result[posa[i] + 4] = veca[i] * vecb[2];
      result[posa[i] + 5] = veca[i] * vecb[3];
    }
    return result;
  }

 private:
  std::complex<double> _data[16];
};

// typedef Matrix4x4<std::complex<double>> MC4x4;
typedef Matrix4x4 MC4x4;

#endif
